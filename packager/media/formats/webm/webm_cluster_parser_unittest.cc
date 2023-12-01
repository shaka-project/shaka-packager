// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/webm/webm_cluster_parser.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/str_format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/macros/classes.h>
#include <packager/media/base/decrypt_config.h>
#include <packager/media/base/raw_key_source.h>
#include <packager/media/base/timestamp.h>
#include <packager/media/formats/webm/cluster_builder.h>
#include <packager/media/formats/webm/webm_constants.h>

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace shaka {
namespace media {

typedef WebMTracksParser::TextTracks TextTracks;
typedef std::map<uint32_t, BufferQueue> TextBufferQueueMap;

// Matchers for verifying common media log entry strings.
MATCHER_P(OpusPacketDurationTooHigh, actual_duration_ms, "") {
  return CONTAINS_STRING(
      arg, "Warning, demuxed Opus packet with encoded duration: " +
               absl::StrFormat("%d", actual_duration_ms) +
               "ms. Should be no greater than 120ms.");
}

MATCHER_P(WebMSimpleBlockDurationEstimated, estimated_duration_ms, "") {
  return CONTAINS_STRING(arg, "Estimating WebM block duration to be " +
                                  absl::StrFormat("%d", estimated_duration_ms) +
                                  "ms for the last (Simple)Block in the "
                                  "Cluster for this Track. Use BlockGroups "
                                  "with BlockDurations at the end of each "
                                  "Track in a Cluster to avoid estimation.");
}

MATCHER_P2(WebMBlockDurationMismatchesOpusDuration,
           block_duration_ms,
           opus_duration_ms,
           "") {
  return CONTAINS_STRING(
      arg, "BlockDuration (" + absl::StrFormat("%d", block_duration_ms) +
               "ms) differs significantly from encoded duration (" +
               absl::StrFormat("%d", opus_duration_ms) + "ms).");
}

namespace {

const int64_t kMicrosecondsPerMillisecond = 1000;
// Timecode scale for millisecond timestamps.
const int kTimecodeScale = 1000000;

const int kAudioTrackNum = 1;
const int kVideoTrackNum = 2;
const int kTextTrackNum = 3;
const int kTestAudioFrameDefaultDurationInMs = 13;
const int kTestVideoFrameDefaultDurationInMs = 17;

// Constants for AudioStreamInfo and VideoStreamInfo. Most are not used.
const int32_t kTimeScale = 1000000;
const int64_t kDuration = 10000000;
const char kCodecString[] = "codec_string";
const char kLanguage[] = "eng";
const uint8_t kBitsPerSample = 8u;
const uint8_t kNumChannels = 2u;
const uint32_t kSamplingFrequency = 48000u;
const uint64_t kSeekPreroll = 0u;
const uint64_t kCodecDelay = 0u;
const uint8_t* kExtraData = nullptr;
const size_t kExtraDataSize = 0u;
const bool kEncrypted = true;
const uint16_t kWidth = 320u;
const uint16_t kHeight = 180u;
const uint32_t kPixelWidth = 1u;
const uint32_t kPixelHeight = 1u;
const uint8_t kTransferCharacteristics = 0;
const int16_t kTrickPlayFactor = 0;
const uint8_t kNaluLengthSize = 0u;

// Test duration defaults must differ from parser estimation defaults to know
// which durations parser used when emitting buffers.
static_assert(
    static_cast<int>(kTestAudioFrameDefaultDurationInMs) !=
        static_cast<int>(WebMClusterParser::kDefaultAudioBufferDurationInMs),
    "test default is the same as estimation fallback audio duration");
static_assert(
    static_cast<int>(kTestVideoFrameDefaultDurationInMs) !=
        static_cast<int>(WebMClusterParser::kDefaultVideoBufferDurationInMs),
    "test default is the same as estimation fallback video duration");

struct BlockInfo {
  int track_num;
  int timestamp;

  // Negative value is allowed only for block groups (not simple blocks) and
  // directs CreateCluster() to exclude BlockDuration entry from the cluster for
  // this BlockGroup. The absolute value is used for parser verification.
  // For simple blocks, this value must be non-negative, and is used only for
  // parser verification.
  double duration;

  bool use_simple_block;

  // Default data will be used if no data given.
  const uint8_t* data;
  int data_length;

  bool is_key_frame;
};

const BlockInfo kDefaultBlockInfo[] = {
    {kAudioTrackNum, 0, 23, true, NULL, 0, true},
    {kAudioTrackNum, 23, 23, true, NULL, 0, true},
    // Assumes not using DefaultDuration
    {kVideoTrackNum, 33, 34, true, NULL, 0, true},
    {kAudioTrackNum, 46, 23, true, NULL, 0, false},
    {kVideoTrackNum, 67, 33, false, NULL, 0, true},
    {kAudioTrackNum, 69, 23, false, NULL, 0, false},
    {kVideoTrackNum, 100, 33, false, NULL, 0, false},
};

const uint8_t kEncryptedFrame[] = {
    // Block is encrypted
    0x01,
    // IV
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    // Some dummy encrypted data
    0x01,
};
const uint8_t kMockKey[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00,
};
const uint8_t kExpectedDecryptedFrame[] = {
    0x41,
};

const uint8_t kClearFrameInEncryptedTrack[] = {
    // Block is not encrypted
    0x00,
    // Some dummy frame data
    0x01, 0x02, 0x03,
};
const uint8_t kExpectedClearFrame[] = {
    0x01, 0x02, 0x03,
};

const uint8_t kVP8Frame[] = {
    0x52, 0x04, 0x00, 0x9d, 0x01, 0x2a, 0x40, 0x01, 0xf0, 0x00, 0x00, 0x47,
    0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x01, 0x24, 0x10, 0x17, 0x67,
    0x63, 0x3f, 0xbb, 0xe5, 0xcf, 0x9b, 0x7d, 0x53, 0xec, 0x67, 0xa2, 0xcf,
};
const uint8_t kVP9Frame[] = {
    0xb1, 0x24, 0xc1, 0xa1, 0x40, 0x00, 0x4f, 0x80, 0x2c, 0xa0, 0x41, 0xc1,
    0x20, 0xe0, 0xc3, 0xf0, 0x00, 0x09, 0x00, 0x7c, 0x57, 0x77, 0x3f, 0x67,
    0x99, 0x3e, 0x1f, 0xfb, 0xdf, 0x0f, 0x02, 0x0a, 0x37, 0x81, 0x53, 0x80,
    0x00, 0x7e, 0x6f, 0xfe, 0x74, 0x31, 0xc6, 0x4f, 0x23, 0x9d, 0x6e, 0x5f,
    0xfc, 0xa8, 0xef, 0x67, 0xdc, 0xac, 0xf7, 0x3e, 0x31, 0x07, 0xab, 0xc7,
    0x0c, 0x74, 0x48, 0x8b, 0x95, 0x30, 0xc9, 0xf0, 0x37, 0x3b, 0xe6, 0x11,
    0xe1, 0xe6, 0xef, 0xff, 0xfd, 0xf7, 0x4f, 0x0f,
};

class MockKeySource : public RawKeySource {
 public:
  MOCK_METHOD2(GetKey,
               Status(const std::vector<uint8_t>& key_id, EncryptionKey* key));
};

std::unique_ptr<Cluster> CreateCluster(int /*timecode*/,
                                       const BlockInfo* block_info,
                                       int block_count) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);

  // Default block data for audio, video and text.
  uint8_t kDefaultBlockData[] = {0x00, 0x0A, 0x01, 0x0D, 0x02};

  for (int i = 0; i < block_count; i++) {
    const uint8_t* data;
    int data_length;
    if (block_info[i].data != NULL) {
      data = block_info[i].data;
      data_length = block_info[i].data_length;
    } else {
      data = kDefaultBlockData;
      data_length = std::size(kDefaultBlockData);
    }

    if (block_info[i].use_simple_block) {
      CHECK_GE(block_info[i].duration, 0);
      cb.AddSimpleBlock(block_info[i].track_num, block_info[i].timestamp,
                        block_info[i].is_key_frame ? 0x80 : 0x00, data,
                        data_length);
      continue;
    }

    if (block_info[i].duration < 0) {
      cb.AddBlockGroupWithoutBlockDuration(
          block_info[i].track_num, block_info[i].timestamp, 0,
          block_info[i].is_key_frame, data, data_length);
      continue;
    }

    cb.AddBlockGroup(block_info[i].track_num, block_info[i].timestamp,
                     block_info[i].duration, 0, block_info[i].is_key_frame,
                     data, data_length);
  }

  return cb.Finish();
}

// Creates a Cluster with one block.
std::unique_ptr<Cluster> CreateCluster(const uint8_t* data, size_t data_size) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  cb.AddSimpleBlock(kVideoTrackNum, 0, 0, data, static_cast<int>(data_size));
  return cb.Finish();
}

bool VerifyBuffersHelper(const BufferQueue& audio_buffers,
                         const BufferQueue& video_buffers,
                         const BufferQueue& text_buffers,
                         const BlockInfo* block_info,
                         int block_count) {
  int buffer_count = static_cast<int>(
      audio_buffers.size() + video_buffers.size() + text_buffers.size());
  if (block_count != buffer_count) {
    LOG(ERROR) << __FUNCTION__ << " : block_count (" << block_count
               << ") mismatches buffer_count (" << buffer_count << ")";
    return false;
  }

  size_t audio_offset = 0;
  size_t video_offset = 0;
  size_t text_offset = 0;
  for (int i = 0; i < block_count; i++) {
    const BufferQueue* buffers = NULL;
    size_t* offset;

    if (block_info[i].track_num == kAudioTrackNum) {
      buffers = &audio_buffers;
      offset = &audio_offset;
    } else if (block_info[i].track_num == kVideoTrackNum) {
      buffers = &video_buffers;
      offset = &video_offset;
    } else if (block_info[i].track_num == kTextTrackNum) {
      buffers = &text_buffers;
      offset = &text_offset;
    } else {
      LOG(ERROR) << "Unexpected track number " << block_info[i].track_num;
      return false;
    }

    if (*offset >= buffers->size()) {
      LOG(ERROR) << __FUNCTION__ << " : Too few buffers (" << buffers->size()
                 << ") for track_num (" << block_info[i].track_num
                 << "), expected at least " << *offset + 1 << " buffers";
      return false;
    }

    std::shared_ptr<MediaSample> buffer = (*buffers)[(*offset)++];

    EXPECT_EQ(block_info[i].timestamp * kMicrosecondsPerMillisecond,
              buffer->pts());
    EXPECT_EQ(std::abs(block_info[i].duration) * kMicrosecondsPerMillisecond,
              buffer->duration());
    EXPECT_EQ(block_info[i].is_key_frame, buffer->is_key_frame());
  }

  return true;
}

bool VerifyTextBuffers(const BlockInfo* block_info_ptr,
                       int block_count,
                       int text_track_num,
                       const BufferQueue& text_buffers) {
  const BlockInfo* const block_info_end = block_info_ptr + block_count;

  typedef BufferQueue::const_iterator TextBufferIter;
  TextBufferIter buffer_iter = text_buffers.begin();
  const TextBufferIter buffer_end = text_buffers.end();

  while (block_info_ptr != block_info_end) {
    const BlockInfo& block_info = *block_info_ptr++;

    if (block_info.track_num != text_track_num)
      continue;

    EXPECT_FALSE(block_info.use_simple_block);
    EXPECT_FALSE(buffer_iter == buffer_end);

    const std::shared_ptr<MediaSample> buffer = *buffer_iter++;
    EXPECT_EQ(block_info.timestamp * kMicrosecondsPerMillisecond,
              buffer->pts());
    EXPECT_EQ(std::abs(block_info.duration) * kMicrosecondsPerMillisecond,
              buffer->duration());
  }

  EXPECT_TRUE(buffer_iter == buffer_end);
  return true;
}

}  // namespace

class WebMClusterParserTest : public testing::Test {
 public:
  WebMClusterParserTest()
      : audio_stream_info_(new AudioStreamInfo(kAudioTrackNum,
                                               kTimeScale,
                                               kDuration,
                                               kUnknownCodec,
                                               kCodecString,
                                               kExtraData,
                                               kExtraDataSize,
                                               kBitsPerSample,
                                               kNumChannels,
                                               kSamplingFrequency,
                                               kSeekPreroll,
                                               kCodecDelay,
                                               0,
                                               0,
                                               kLanguage,
                                               !kEncrypted)),
        video_stream_info_(new VideoStreamInfo(kVideoTrackNum,
                                               kTimeScale,
                                               kDuration,
                                               kCodecVP8,
                                               H26xStreamFormat::kUnSpecified,
                                               kCodecString,
                                               kExtraData,
                                               kExtraDataSize,
                                               kWidth,
                                               kHeight,
                                               kPixelWidth,
                                               kPixelHeight,
                                               kTransferCharacteristics,
                                               kTrickPlayFactor,
                                               kNaluLengthSize,
                                               kLanguage,
                                               !kEncrypted)),
        parser_(CreateDefaultParser()) {}

 protected:
  void ResetParserToHaveDefaultDurations() {
    int64_t default_audio_duration =
        kTestAudioFrameDefaultDurationInMs * kMicrosecondsPerMillisecond;
    int64_t default_video_duration =
        kTestVideoFrameDefaultDurationInMs * kMicrosecondsPerMillisecond;
    ASSERT_GE(default_audio_duration, 0);
    ASSERT_GE(default_video_duration, 0);
    ASSERT_NE(kNoTimestamp, default_audio_duration);
    ASSERT_NE(kNoTimestamp, default_video_duration);

    parser_.reset(CreateParserWithDefaultDurationsAndOptionalTextTracks(
        default_audio_duration, default_video_duration));
  }

  void InitEvent(const std::vector<std::shared_ptr<StreamInfo>>& stream_info) {
    streams_from_init_event_ = stream_info;
  }

  bool NewSampleEvent(uint32_t track_id, std::shared_ptr<MediaSample> sample) {
    switch (track_id) {
      case kAudioTrackNum:
        audio_buffers_.push_back(sample);
        break;
      case kVideoTrackNum:
        video_buffers_.push_back(sample);
        break;
      case kTextTrackNum:
      case kTextTrackNum + 1:
        text_buffers_map_[track_id].push_back(sample);
        break;
      default:
        LOG(ERROR) << "Unexpected track number " << track_id;
        return false;
    }
    return true;
  }

  // Helper that hard-codes some non-varying constructor parameters.
  WebMClusterParser* CreateParserHelper(
      int64_t audio_default_duration, int64_t video_default_duration,
      const WebMTracksParser::TextTracks& text_tracks,
      const std::set<int64_t>& ignored_tracks,
      const std::string& audio_encryption_key_id,
      const std::string& video_encryption_key_id, const Codec audio_codec,
      const Codec video_codec, const MediaParser::InitCB& init_cb) {
    using namespace std::placeholders;
    audio_stream_info_->set_codec(audio_codec);
    video_stream_info_->set_codec(video_codec);
    return new WebMClusterParser(
        kTimecodeScale, audio_stream_info_, video_stream_info_,
        VPCodecConfigurationRecord(), audio_default_duration,
        video_default_duration, text_tracks, ignored_tracks,
        audio_encryption_key_id, video_encryption_key_id,
        std::bind(&WebMClusterParserTest::NewSampleEvent, this, _1, _2),
        init_cb, &mock_key_source_);
  }

  // Create a default version of the parser for test.
  WebMClusterParser* CreateDefaultParser() {
    return CreateParserHelper(kNoTimestamp, kNoTimestamp, TextTracks(),
                              std::set<int64_t>(), std::string(), std::string(),
                              kUnknownCodec, kCodecVP8, MediaParser::InitCB());
  }

  // Create a parser for test with custom audio and video default durations, and
  // optionally custom text tracks.
  WebMClusterParser* CreateParserWithDefaultDurationsAndOptionalTextTracks(
      int64_t audio_default_duration,
      int64_t video_default_duration,
      const WebMTracksParser::TextTracks& text_tracks = TextTracks()) {
    return CreateParserHelper(audio_default_duration, video_default_duration,
                              text_tracks, std::set<int64_t>(), std::string(),
                              std::string(), kUnknownCodec, kCodecVP8,
                              MediaParser::InitCB());
  }

  // Create a parser for test with custom ignored tracks.
  WebMClusterParser* CreateParserWithIgnoredTracks(
      std::set<int64_t>& ignored_tracks) {
    return CreateParserHelper(kNoTimestamp, kNoTimestamp, TextTracks(),
                              ignored_tracks, std::string(), std::string(),
                              kUnknownCodec, kCodecVP8, MediaParser::InitCB());
  }

  // Create a parser for test with custom encryption key ids and audio codec.
  WebMClusterParser* CreateParserWithKeyIdsAndCodec(
      const std::string& audio_encryption_key_id,
      const std::string& video_encryption_key_id, const Codec audio_codec) {
    return CreateParserHelper(kNoTimestamp, kNoTimestamp, TextTracks(),
                              std::set<int64_t>(), audio_encryption_key_id,
                              video_encryption_key_id, audio_codec, kCodecVP8,
                              MediaParser::InitCB());
  }

  // Create a parser for test with custom video codec, also check for init
  // events.
  WebMClusterParser* CreateParserWithCodec(const Codec video_codec) {
    using namespace std::placeholders;
    return CreateParserHelper(
        kNoTimestamp, kNoTimestamp, TextTracks(), std::set<int64_t>(),
        std::string(), std::string(), kUnknownCodec, video_codec,
        std::bind(&WebMClusterParserTest::InitEvent, this, _1));
  }

  bool VerifyBuffers(const BlockInfo* block_info, int block_count) {
    bool result = VerifyBuffersHelper(audio_buffers_, video_buffers_,
                                      text_buffers_map_[kTextTrackNum],
                                      block_info, block_count);
    audio_buffers_.clear();
    video_buffers_.clear();
    text_buffers_map_.clear();
    return result;
  }

  std::shared_ptr<AudioStreamInfo> audio_stream_info_;
  std::shared_ptr<VideoStreamInfo> video_stream_info_;
  std::unique_ptr<WebMClusterParser> parser_;
  std::vector<std::shared_ptr<StreamInfo>> streams_from_init_event_;
  BufferQueue audio_buffers_;
  BufferQueue video_buffers_;
  TextBufferQueueMap text_buffers_map_;
  StrictMock<MockKeySource> mock_key_source_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMClusterParserTest);
};

TEST_F(WebMClusterParserTest, TracksWithSampleMissingDuration) {
  InSequence s;

  // Reset the parser to have 3 tracks: text, video (no default frame duration),
  // and audio (with a default frame duration).
  TextTracks text_tracks;
  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));
  int64_t default_audio_duration = kTestAudioFrameDefaultDurationInMs;
  ASSERT_GE(default_audio_duration, 0);
  ASSERT_NE(kNoTimestamp, default_audio_duration);
  parser_.reset(CreateParserWithDefaultDurationsAndOptionalTextTracks(
      default_audio_duration * kMicrosecondsPerMillisecond, kNoTimestamp,
      text_tracks));

  const int kExpectedVideoEstimationInMs = 33;

  const BlockInfo kBlockInfo[] = {
      // Note that for simple blocks, duration is not encoded.
      {kVideoTrackNum, 0, 0, true, NULL, 0, false},
      {kAudioTrackNum, 0, 23, false, NULL, 0, false},
      {kTextTrackNum, 10, 42, false, NULL, 0, true},
      {kAudioTrackNum, 23, 0, true, NULL, 0, false},
      {kVideoTrackNum, 33, 0, true, NULL, 0, false},
      {kAudioTrackNum, 36, 0, true, NULL, 0, false},
      {kVideoTrackNum, 66, 0, true, NULL, 0, false},
      {kAudioTrackNum, 70, 0, true, NULL, 0, false},
      {kAudioTrackNum, 83, 0, true, NULL, 0, false},
  };

  // Samples are not emitted in the same order as |kBlockInfo| due to missing of
  // duration in some samples.
  const BlockInfo kExpectedBlockInfo[] = {
      {kAudioTrackNum, 0, 23, false, NULL, 0, false},
      {kTextTrackNum, 10, 42, false, NULL, 0, true},
      {kVideoTrackNum, 0, 33, true, NULL, 0, false},
      {kAudioTrackNum, 23, 13, true, NULL, 0, false},
      {kVideoTrackNum, 33, 33, true, NULL, 0, false},
      {kAudioTrackNum, 36, 34, true, NULL, 0, false},
      {kAudioTrackNum, 70, 13, true, NULL, 0, false},
      {kAudioTrackNum, 83, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kVideoTrackNum, 66, kExpectedVideoEstimationInMs, true, NULL, 0, false},
  };
  const int kExpectedBuffersOnPartialCluster[] = {
    0,  // Video simple block without duration should be held back
    1,  // 1st audio buffer ready
    2,  // Text buffer ready
    2,  // Audio simple block without duration should be held back
    3,  // 1st video emitted, 2nd video held back with no duration
    4,  // 2rd audio ready, 3rd audio held back with no duration
    5,  // 2nd video emitted, 3rd video held back with no duration
    6,  // 3th audio ready, 4th audio held back with no duration
    7,  // 4th audio ready, 5th audio held back with no duration
  };

  ASSERT_EQ(std::size(kBlockInfo), std::size(kExpectedBuffersOnPartialCluster));
  int block_count = std::size(kBlockInfo);

  // Iteratively create a cluster containing the first N+1 blocks and parse the
  // cluster. Verify that the corresponding entry in
  // |kExpectedBuffersOnPartialCluster| identifies the exact subset of
  // |kBlockInfo| returned by the parser.
  for (int i = 0; i < block_count; ++i) {
    parser_->Reset();

    const int blocks_in_cluster = i + 1;
    std::unique_ptr<Cluster> cluster(
        CreateCluster(0, kBlockInfo, blocks_in_cluster));

    EXPECT_EQ(cluster->size(),
              parser_->Parse(cluster->data(), cluster->size()));
    EXPECT_TRUE(
        VerifyBuffers(kExpectedBlockInfo, kExpectedBuffersOnPartialCluster[i]));
  }

  // The last audio (5th) and the last video (3rd) are emitted on flush with
  // duration estimated - estimated to be default duration if it is specified,
  // otherwise estimated from earlier frames.
  EXPECT_TRUE(parser_->Flush());
  EXPECT_TRUE(VerifyBuffers(&kExpectedBlockInfo[block_count - 2], 2));
}

TEST_F(WebMClusterParserTest, Reset) {
  InSequence s;

  int block_count = std::size(kDefaultBlockInfo);
  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kDefaultBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed.
  int result = parser_->Parse(cluster->data(), cluster->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->size());

  ASSERT_TRUE(VerifyBuffers(kDefaultBlockInfo, block_count - 1));
  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithSingleCall) {
  int block_count = std::size(kDefaultBlockInfo);
  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kDefaultBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithMultipleCalls) {
  int block_count = std::size(kDefaultBlockInfo);
  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kDefaultBlockInfo, block_count));

  BufferQueue audio_buffers;
  BufferQueue video_buffers;
  const BufferQueue no_text_buffers;

  const uint8_t* data = cluster->data();
  int size = cluster->size();
  int default_parse_size = 3;
  int parse_size = std::min(default_parse_size, size);

  while (size > 0) {
    int result = parser_->Parse(data, parse_size);
    ASSERT_GE(result, 0);
    ASSERT_LE(result, parse_size);

    if (result == 0) {
      // The parser needs more data so increase the parse_size a little.
      parse_size += default_parse_size;
      parse_size = std::min(parse_size, size);
      continue;
    }

    parse_size = default_parse_size;

    data += result;
    size -= result;
  }
  ASSERT_TRUE(VerifyBuffers(kDefaultBlockInfo, block_count));
}

// Verify that both BlockGroups with the BlockDuration before the Block
// and BlockGroups with the BlockDuration after the Block are supported
// correctly.
// Note: Raw bytes are use here because ClusterBuilder only generates
// one of these scenarios.
TEST_F(WebMClusterParserTest, ParseBlockGroup) {
  const BlockInfo kBlockInfo[] = {
      {kAudioTrackNum, 0, 23, false, NULL, 0, true},
      {kVideoTrackNum, 33, 34, false, NULL, 0, true},
  };
  int block_count = std::size(kBlockInfo);

  const uint8_t kClusterData[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x9B,  // Cluster(size=27)
    0xE7, 0x81, 0x00,  // Timecode(size=1, value=0)
    // BlockGroup with BlockDuration before Block.
    0xA0, 0x8A,  // BlockGroup(size=10)
    0x9B, 0x81, 0x17,  // BlockDuration(size=1, value=23)
    0xA1, 0x85, 0x81, 0x00, 0x00, 0x00, 0xaa,  // Block(size=5, track=1, ts=0)
    // BlockGroup with BlockDuration after Block.
    0xA0, 0x8A,  // BlockGroup(size=10)
    0xA1, 0x85, 0x82, 0x00, 0x21, 0x00, 0x55,  // Block(size=5, track=2, ts=33)
    0x9B, 0x81, 0x22,  // BlockDuration(size=1, value=34)
  };
  const int kClusterSize = std::size(kClusterData);

  int result = parser_->Parse(kClusterData, kClusterSize);
  EXPECT_EQ(kClusterSize, result);
  ASSERT_TRUE(VerifyBuffers(kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseSimpleBlockAndBlockGroupMixture) {
  const BlockInfo kBlockInfo[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, false, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kAudioTrackNum, 46, 23, false, NULL, 0, false},
      {kVideoTrackNum, 67, 33, false, NULL, 0, false},
  };
  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, IgnoredTracks) {
  std::set<int64_t> ignored_tracks;
  ignored_tracks.insert(kTextTrackNum);

  parser_.reset(CreateParserWithIgnoredTracks(ignored_tracks));

  const BlockInfo kInputBlockInfo[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, true, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kTextTrackNum, 33, 99, true, NULL, 0, false},
      {kAudioTrackNum, 46, 23, true, NULL, 0, false},
      {kVideoTrackNum, 67, 34, true, NULL, 0, false},
  };
  int input_block_count = std::size(kInputBlockInfo);

  const BlockInfo kOutputBlockInfo[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, true, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kAudioTrackNum, 46, 23, true, NULL, 0, false},
      {kVideoTrackNum, 67, 34, true, NULL, 0, false},
  };
  int output_block_count = std::size(kOutputBlockInfo);

  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  EXPECT_TRUE(parser_->Flush());
  ASSERT_TRUE(VerifyBuffers(kOutputBlockInfo, output_block_count));
}

TEST_F(WebMClusterParserTest, ParseTextTracks) {
  TextTracks text_tracks;

  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));

  parser_.reset(CreateParserWithDefaultDurationsAndOptionalTextTracks(
      kNoTimestamp, kNoTimestamp, text_tracks));

  const BlockInfo kInputBlockInfo[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, true, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kTextTrackNum, 33, 42, false, NULL, 0, true},
      {kAudioTrackNum, 46, 23, true, NULL, 0, false},
      {kTextTrackNum, 55, 44, false, NULL, 0, true},
      {kVideoTrackNum, 67, 34, true, NULL, 0, false},
  };
  int input_block_count = std::size(kInputBlockInfo);

  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  EXPECT_TRUE(parser_->Flush());
  ASSERT_TRUE(VerifyBuffers(kInputBlockInfo, input_block_count));
}

TEST_F(WebMClusterParserTest, TextTracksSimpleBlock) {
  TextTracks text_tracks;

  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));

  parser_.reset(CreateParserWithDefaultDurationsAndOptionalTextTracks(
      kNoTimestamp, kNoTimestamp, text_tracks));

  const BlockInfo kInputBlockInfo[] = {
      {kTextTrackNum, 33, 42, true, NULL, 0, false},
  };
  int input_block_count = std::size(kInputBlockInfo);

  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_LT(result, 0);
}

TEST_F(WebMClusterParserTest, ParseMultipleTextTracks) {
  TextTracks text_tracks;

  const int kSubtitleTextTrackNum = kTextTrackNum;
  const int kCaptionTextTrackNum = kTextTrackNum + 1;

  text_tracks.insert(std::make_pair(TextTracks::key_type(kSubtitleTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));

  text_tracks.insert(std::make_pair(TextTracks::key_type(kCaptionTextTrackNum),
                                    TextTrackConfig(kTextCaptions, "", "",
                                                    "")));

  parser_.reset(CreateParserWithDefaultDurationsAndOptionalTextTracks(
      kNoTimestamp, kNoTimestamp, text_tracks));

  const BlockInfo kInputBlockInfo[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, true, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kSubtitleTextTrackNum, 33, 42, false, NULL, 0, false},
      {kAudioTrackNum, 46, 23, true, NULL, 0, false},
      {kCaptionTextTrackNum, 55, 44, false, NULL, 0, false},
      {kVideoTrackNum, 67, 34, true, NULL, 0, false},
      {kSubtitleTextTrackNum, 67, 33, false, NULL, 0, false},
  };
  int input_block_count = std::size(kInputBlockInfo);

  std::unique_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);

  for (TextBufferQueueMap::const_iterator itr = text_buffers_map_.begin();
       itr != text_buffers_map_.end(); ++itr) {
    const TextTracks::const_iterator find_result =
        text_tracks.find(itr->first);
    ASSERT_TRUE(find_result != text_tracks.end());
    ASSERT_TRUE(VerifyTextBuffers(kInputBlockInfo, input_block_count,
                                  itr->first, itr->second));
  }
}

TEST_F(WebMClusterParserTest, ParseVP8) {
  std::unique_ptr<Cluster> cluster(
      CreateCluster(kVP8Frame, std::size(kVP8Frame)));
  parser_.reset(CreateParserWithCodec(kCodecVP8));

  EXPECT_EQ(cluster->size(), parser_->Parse(cluster->data(), cluster->size()));

  ASSERT_EQ(2u, streams_from_init_event_.size());
  EXPECT_EQ(kStreamAudio, streams_from_init_event_[0]->stream_type());
  EXPECT_EQ(kStreamVideo, streams_from_init_event_[1]->stream_type());
  EXPECT_EQ("vp08.01.10.08.01.02.02.02.00",
            streams_from_init_event_[1]->codec_string());
}

TEST_F(WebMClusterParserTest, ParseVP9) {
  std::unique_ptr<Cluster> cluster(
      CreateCluster(kVP9Frame, std::size(kVP9Frame)));
  parser_.reset(CreateParserWithCodec(kCodecVP9));

  EXPECT_EQ(cluster->size(), parser_->Parse(cluster->data(), cluster->size()));

  ASSERT_EQ(2u, streams_from_init_event_.size());
  EXPECT_EQ(kStreamAudio, streams_from_init_event_[0]->stream_type());
  EXPECT_EQ(kStreamVideo, streams_from_init_event_[1]->stream_type());
  EXPECT_EQ("vp09.03.10.12.03.02.02.02.00",
            streams_from_init_event_[1]->codec_string());
}

TEST_F(WebMClusterParserTest, ParseEncryptedBlock) {
  const std::string video_key_id("video_key_id");

  EncryptionKey encryption_key;
  encryption_key.key.assign(kMockKey, kMockKey + std::size(kMockKey));
  EXPECT_CALL(
      mock_key_source_,
      GetKey(std::vector<uint8_t>(video_key_id.begin(), video_key_id.end()), _))
      .WillOnce(DoAll(SetArgPointee<1>(encryption_key), Return(Status::OK)));

  std::unique_ptr<Cluster> cluster(
      CreateCluster(kEncryptedFrame, std::size(kEncryptedFrame)));

  parser_.reset(CreateParserWithKeyIdsAndCodec(std::string(), video_key_id,
                                               kUnknownCodec));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  EXPECT_TRUE(parser_->Flush());
  ASSERT_EQ(1UL, video_buffers_.size());
  std::shared_ptr<MediaSample> buffer = video_buffers_[0];
  EXPECT_EQ(std::vector<uint8_t>(
                kExpectedDecryptedFrame,
                kExpectedDecryptedFrame + std::size(kExpectedDecryptedFrame)),
            std::vector<uint8_t>(buffer->data(),
                                 buffer->data() + buffer->data_size()));
}

TEST_F(WebMClusterParserTest, ParseEncryptedBlockGetKeyFailed) {
  EXPECT_CALL(mock_key_source_, GetKey(_, _)).WillOnce(Return(Status::UNKNOWN));

  std::unique_ptr<Cluster> cluster(
      CreateCluster(kEncryptedFrame, std::size(kEncryptedFrame)));

  parser_.reset(CreateParserWithKeyIdsAndCodec(std::string(), "video_key_id",
                                               kUnknownCodec));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(-1, result);
}

TEST_F(WebMClusterParserTest, ParseBadEncryptedBlock) {
  std::unique_ptr<Cluster> cluster(
      CreateCluster(kEncryptedFrame, std::size(kEncryptedFrame) - 2));

  parser_.reset(CreateParserWithKeyIdsAndCodec(std::string(), "video_key_id",
                                               kUnknownCodec));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(-1, result);
}

TEST_F(WebMClusterParserTest, ParseClearFrameInEncryptedTrack) {
  std::unique_ptr<Cluster> cluster(CreateCluster(
      kClearFrameInEncryptedTrack, std::size(kClearFrameInEncryptedTrack)));

  parser_.reset(CreateParserWithKeyIdsAndCodec(std::string(), "video_key_id",
                                               kUnknownCodec));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  EXPECT_TRUE(parser_->Flush());
  ASSERT_EQ(1UL, video_buffers_.size());
  std::shared_ptr<MediaSample> buffer = video_buffers_[0];
  EXPECT_EQ(std::vector<uint8_t>(
                kExpectedClearFrame,
                kExpectedClearFrame + std::size(kExpectedClearFrame)),
            std::vector<uint8_t>(buffer->data(),
                                 buffer->data() + buffer->data_size()));
}

TEST_F(WebMClusterParserTest, ParseInvalidZeroSizedCluster) {
  const uint8_t kBuffer[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x80,  // CLUSTER (size = 0)
  };

  EXPECT_EQ(-1, parser_->Parse(kBuffer, std::size(kBuffer)));
  // Verify init event not called.
  ASSERT_EQ(0u, streams_from_init_event_.size());
}

TEST_F(WebMClusterParserTest, ParseInvalidUnknownButActuallyZeroSizedCluster) {
  const uint8_t kBuffer[] = {
    0x1F, 0x43, 0xB6, 0x75, 0xFF,  // CLUSTER (size = "unknown")
    0x1F, 0x43, 0xB6, 0x75, 0x85,  // CLUSTER (size = 5)
  };

  EXPECT_EQ(-1, parser_->Parse(kBuffer, std::size(kBuffer)));
}

TEST_F(WebMClusterParserTest, ParseInvalidTextBlockGroupWithoutDuration) {
  // Text track frames must have explicitly specified BlockGroup BlockDurations.
  TextTracks text_tracks;

  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));

  parser_.reset(CreateParserWithDefaultDurationsAndOptionalTextTracks(
      kNoTimestamp, kNoTimestamp, text_tracks));

  const BlockInfo kBlockInfo[] = {
      {kTextTrackNum, 33, -42, false, NULL, 0, false},
  };
  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_LT(result, 0);
}

TEST_F(WebMClusterParserTest, ParseWithDefaultDurationsSimpleBlocks) {
  InSequence s;
  ResetParserToHaveDefaultDurations();

  EXPECT_LT(kTestAudioFrameDefaultDurationInMs, 23);
  EXPECT_LT(kTestVideoFrameDefaultDurationInMs, 33);

  const BlockInfo kBlockInfo[] = {
      // Note that for simple blocks, duration is not encoded.
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 23, true, NULL, 0, false},
      {kVideoTrackNum, 33, 34, true, NULL, 0, false},
      {kAudioTrackNum, 46, 23, true, NULL, 0, false},
      {kVideoTrackNum, 67, 33, true, NULL, 0, false},
      {kAudioTrackNum, 69, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kVideoTrackNum, 100, kTestVideoFrameDefaultDurationInMs, true, NULL, 0,
       false},
  };

  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  // Now parse a whole cluster to verify that all the blocks will get parsed
  // and the last audio and video are held back due to no duration.
  // The durations for all blocks are calculated to be the timestamp difference
  // with the next block.
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(kBlockInfo, block_count - 2));
  // The last audio and video are emitted on flush wiht duration estimated -
  // estimated to be default_duration since it is present.
  EXPECT_TRUE(parser_->Flush());
  ASSERT_TRUE(VerifyBuffers(&kBlockInfo[block_count - 2], 2));
}

TEST_F(WebMClusterParserTest, ParseWithoutAnyDurationsSimpleBlocks) {
  InSequence s;

  // Absent DefaultDuration information, SimpleBlock durations are derived from
  // inter-buffer track timestamp delta either within or across clusters.
  // Duration for the last block is estimated independently for each track when
  // Flush() is called. We use the maximum seen so far for estimation.

  const BlockInfo kBlockInfo1[] = {
      {kAudioTrackNum, 0, 23, true, NULL, 0, false},
      {kAudioTrackNum, 23, 22, true, NULL, 0, false},
      {kVideoTrackNum, 33, 33, true, NULL, 0, false},
      {kAudioTrackNum, 45, 23, true, NULL, 0, false},
      {kVideoTrackNum, 66, 34, true, NULL, 0, false},
      {kAudioTrackNum, 68, 24, true, NULL, 0, false},
      {kVideoTrackNum, 100, 35, true, NULL, 0, false},
  };

  int block_count1 = std::size(kBlockInfo1);
  std::unique_ptr<Cluster> cluster1(
      CreateCluster(0, kBlockInfo1, block_count1));

  // Verify the last fully parsed audio and video buffer are both missing from
  // the result (parser should hold them aside for duration estimation until
  // Flush() called in the absence of DefaultDurations).
  EXPECT_EQ(cluster1->size(),
            parser_->Parse(cluster1->data(), cluster1->size()));
  EXPECT_EQ(3UL, audio_buffers_.size());
  EXPECT_EQ(2UL, video_buffers_.size());
  ASSERT_TRUE(VerifyBuffers(kBlockInfo1, block_count1 - 2));

  // Verify that the estimated frame duration is tracked across clusters for
  // each track.
  const int kExpectedAudioEstimationInMs = 24;
  const int kExpectedVideoEstimationInMs = 35;
  const BlockInfo kBlockInfo2[] = {
      {kAudioTrackNum, 92, kExpectedAudioEstimationInMs, true, NULL, 0, false},
      {kVideoTrackNum, 135, kExpectedVideoEstimationInMs, true, NULL, 0, false},
  };

  int block_count2 = std::size(kBlockInfo2);
  std::unique_ptr<Cluster> cluster2(
      CreateCluster(0, kBlockInfo2, block_count2));
  EXPECT_EQ(cluster2->size(),
            parser_->Parse(cluster2->data(), cluster2->size()));

  // Verify that remaining blocks of cluster1 are emitted.
  ASSERT_TRUE(VerifyBuffers(&kBlockInfo1[block_count1 - 2], 2));

  // Now flush and verify blocks in cluster2 are emitted.
  EXPECT_TRUE(parser_->Flush());
  ASSERT_TRUE(VerifyBuffers(kBlockInfo2, block_count2));
}

TEST_F(WebMClusterParserTest, ParseWithoutAnyDurationsBlockGroups) {
  InSequence s;

  // Absent DefaultDuration and BlockDuration information, BlockGroup block
  // durations are derived from inter-buffer track timestamp delta either within
  // or across clusters. Duration for the last block is estimated independently
  // for each track when Flush() is called. We use the maximum seen so far.

  const BlockInfo kBlockInfo1[] = {
      {kAudioTrackNum, 0, -23, false, NULL, 0, false},
      {kAudioTrackNum, 23, -22, false, NULL, 0, false},
      {kVideoTrackNum, 33, -33, false, NULL, 0, false},
      {kAudioTrackNum, 45, -23, false, NULL, 0, false},
      {kVideoTrackNum, 66, -34, false, NULL, 0, false},
      {kAudioTrackNum, 68, -24, false, NULL, 0, false},
      {kVideoTrackNum, 100, -35, false, NULL, 0, false},
  };

  int block_count1 = std::size(kBlockInfo1);
  std::unique_ptr<Cluster> cluster1(
      CreateCluster(0, kBlockInfo1, block_count1));

  // Verify the last fully parsed audio and video buffer are both missing from
  // the result (parser should hold them aside for duration estimation until
  // Flush() called in the absence of DefaultDurations).
  EXPECT_EQ(cluster1->size(),
            parser_->Parse(cluster1->data(), cluster1->size()));
  EXPECT_EQ(3UL, audio_buffers_.size());
  EXPECT_EQ(2UL, video_buffers_.size());
  ASSERT_TRUE(VerifyBuffers(kBlockInfo1, block_count1 - 2));

  // Verify that the estimated frame duration is tracked across clusters for
  // each track.
  const int kExpectedAudioEstimationInMs = 24;
  const int kExpectedVideoEstimationInMs = 35;
  const BlockInfo kBlockInfo2[] = {
      {kAudioTrackNum, 92, -kExpectedAudioEstimationInMs, false, NULL, 0,
       false},
      {kVideoTrackNum, 135, -kExpectedVideoEstimationInMs, false, NULL, 0,
       false},
  };

  int block_count2 = std::size(kBlockInfo2);
  std::unique_ptr<Cluster> cluster2(
      CreateCluster(0, kBlockInfo2, block_count2));
  EXPECT_EQ(cluster2->size(),
            parser_->Parse(cluster2->data(), cluster2->size()));

  // Verify that remaining blocks of cluster1 are emitted.
  ASSERT_TRUE(VerifyBuffers(&kBlockInfo1[block_count1 - 2], 2));

  // Now flush and verify blocks in cluster2 are emitted.
  EXPECT_TRUE(parser_->Flush());
  ASSERT_TRUE(VerifyBuffers(kBlockInfo2, block_count2));
}

TEST_F(WebMClusterParserTest,
       ParseDegenerateClusterYieldsHardcodedEstimatedDurations) {
  const BlockInfo kBlockInfo[] = {
      {kAudioTrackNum, 0, WebMClusterParser::kDefaultAudioBufferDurationInMs,
       true, NULL, 0, false},
      {kVideoTrackNum, 0, WebMClusterParser::kDefaultVideoBufferDurationInMs,
       true, NULL, 0, false},
  };

  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  EXPECT_TRUE(parser_->Flush());
  ASSERT_TRUE(VerifyBuffers(kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest,
       ParseDegenerateClusterWithDefaultDurationsYieldsDefaultDurations) {
  ResetParserToHaveDefaultDurations();

  const BlockInfo kBlockInfo[] = {
      {kAudioTrackNum, 0, kTestAudioFrameDefaultDurationInMs, true, NULL, 0,
       false},
      {kVideoTrackNum, 0, kTestVideoFrameDefaultDurationInMs, true, NULL, 0,
       false},
  };

  int block_count = std::size(kBlockInfo);
  std::unique_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  EXPECT_TRUE(parser_->Flush());
  ASSERT_TRUE(VerifyBuffers(kBlockInfo, block_count));
}

}  // namespace media
}  // namespace shaka
