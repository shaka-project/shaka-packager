// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/decrypt_config.h"
#include "media/webm/cluster_builder.h"
#include "media/webm/webm_cluster_parser.h"
#include "media/webm/webm_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace media {

enum {
  kTimecodeScale = 1000000,  // Timecode scale for millisecond timestamps.
  kAudioTrackNum = 1,
  kVideoTrackNum = 2,
  kTextTrackNum = 3,
};

struct BlockInfo {
  int track_num;
  int timestamp;
  int duration;
  bool use_simple_block;
};

static const BlockInfo kDefaultBlockInfo[] = {
  { kAudioTrackNum, 0, 23, true },
  { kAudioTrackNum, 23, 23, true },
  { kVideoTrackNum, 33, 34, true },
  { kAudioTrackNum, 46, 23, true },
  { kVideoTrackNum, 67, 33, false },
  { kAudioTrackNum, 69, 23, false },
  { kVideoTrackNum, 100, 33, false },
};

static const uint8 kEncryptedFrame[] = {
  0x01,  // Block is encrypted
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08  // IV
};

static scoped_ptr<Cluster> CreateCluster(int timecode,
                                         const BlockInfo* block_info,
                                         int block_count) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);

  for (int i = 0; i < block_count; i++) {
    uint8 data[] = { 0x00 };
    if (block_info[i].use_simple_block) {
      cb.AddSimpleBlock(block_info[i].track_num,
                        block_info[i].timestamp,
                        0, data, sizeof(data));
      continue;
    }

    CHECK_GE(block_info[i].duration, 0);
    cb.AddBlockGroup(block_info[i].track_num,
                     block_info[i].timestamp,
                     block_info[i].duration,
                     0, data, sizeof(data));
  }

  return cb.Finish();
}

// Creates a Cluster with one encrypted Block. |bytes_to_write| is number of
// bytes of the encrypted frame to write.
static scoped_ptr<Cluster> CreateEncryptedCluster(int bytes_to_write) {
  CHECK_GT(bytes_to_write, 0);
  CHECK_LE(bytes_to_write, static_cast<int>(sizeof(kEncryptedFrame)));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  cb.AddSimpleBlock(kVideoTrackNum, 0, 0, kEncryptedFrame, bytes_to_write);
  return cb.Finish();
}

static bool VerifyBuffers(const WebMClusterParser::BufferQueue& audio_buffers,
                          const WebMClusterParser::BufferQueue& video_buffers,
                          const WebMClusterParser::BufferQueue& text_buffers,
                          const BlockInfo* block_info,
                          int block_count) {
  size_t audio_offset = 0;
  size_t video_offset = 0;
  size_t text_offset = 0;
  for (int i = 0; i < block_count; i++) {
    const WebMClusterParser::BufferQueue* buffers = NULL;
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

    if (*offset >= buffers->size())
      return false;

    scoped_refptr<StreamParserBuffer> buffer = (*buffers)[(*offset)++];


    EXPECT_EQ(buffer->timestamp().InMilliseconds(), block_info[i].timestamp);

    if (!block_info[i].use_simple_block)
      EXPECT_NE(buffer->duration(), kNoTimestamp());

    if (buffer->duration() != kNoTimestamp())
      EXPECT_EQ(buffer->duration().InMilliseconds(), block_info[i].duration);
  }

  return true;
}

static bool VerifyBuffers(const scoped_ptr<WebMClusterParser>& parser,
                          const BlockInfo* block_info,
                          int block_count) {
  typedef WebMClusterParser::TextTrackIterator TextTrackIterator;
  TextTrackIterator text_it = parser->CreateTextTrackIterator();

  int text_track_num;
  const WebMClusterParser::BufferQueue* text_buffers;

  while (text_it(&text_track_num, &text_buffers))
    break;

  const WebMClusterParser::BufferQueue no_text_buffers;

  if (text_buffers == NULL)
    text_buffers = &no_text_buffers;

  return VerifyBuffers(parser->audio_buffers(),
                       parser->video_buffers(),
                       *text_buffers,
                       block_info,
                       block_count);
}

static bool VerifyTextBuffers(
    const scoped_ptr<WebMClusterParser>& parser,
    const BlockInfo* block_info_ptr,
    int block_count,
    int text_track_num,
    const WebMClusterParser::BufferQueue& text_buffers) {
  const BlockInfo* const block_info_end = block_info_ptr + block_count;

  typedef WebMClusterParser::BufferQueue::const_iterator TextBufferIter;
  TextBufferIter buffer_iter = text_buffers.begin();
  const TextBufferIter buffer_end = text_buffers.end();

  while (block_info_ptr != block_info_end) {
    const BlockInfo& block_info = *block_info_ptr++;

    if (block_info.track_num != text_track_num)
      continue;

    EXPECT_FALSE(block_info.use_simple_block);
    EXPECT_FALSE(buffer_iter == buffer_end);

    const scoped_refptr<StreamParserBuffer> buffer = *buffer_iter++;
    EXPECT_EQ(buffer->timestamp().InMilliseconds(), block_info.timestamp);
    EXPECT_EQ(buffer->duration().InMilliseconds(), block_info.duration);
  }

  EXPECT_TRUE(buffer_iter == buffer_end);
  return true;
}

static bool VerifyEncryptedBuffer(
    scoped_refptr<StreamParserBuffer> buffer) {
  EXPECT_TRUE(buffer->decrypt_config());
  EXPECT_EQ(static_cast<unsigned long>(DecryptConfig::kDecryptionKeySize),
            buffer->decrypt_config()->iv().length());
  const uint8* data = buffer->data();
  return data[0] & kWebMFlagEncryptedFrame;
}

static void AppendToEnd(const WebMClusterParser::BufferQueue& src,
                        WebMClusterParser::BufferQueue* dest) {
  for (WebMClusterParser::BufferQueue::const_iterator itr = src.begin();
       itr != src.end(); ++itr) {
    dest->push_back(*itr);
  }
}

class WebMClusterParserTest : public testing::Test {
 public:
  WebMClusterParserTest()
      : parser_(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kVideoTrackNum,
                                      WebMTracksParser::TextTracks(),
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB())) {}

 protected:
  scoped_ptr<WebMClusterParser> parser_;
};

TEST_F(WebMClusterParserTest, Reset) {
  InSequence s;

  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed.
  int result = parser_->Parse(cluster->data(), cluster->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->size());

  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count - 1));
  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(result, cluster->size());
  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithSingleCall) {
  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithMultipleCalls) {
  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  WebMClusterParser::BufferQueue audio_buffers;
  WebMClusterParser::BufferQueue video_buffers;
  const WebMClusterParser::BufferQueue no_text_buffers;

  const uint8* data = cluster->data();
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

    AppendToEnd(parser_->audio_buffers(), &audio_buffers);
    AppendToEnd(parser_->video_buffers(), &video_buffers);

    parse_size = default_parse_size;

    data += result;
    size -= result;
  }
  ASSERT_TRUE(VerifyBuffers(audio_buffers, video_buffers,
                            no_text_buffers, kDefaultBlockInfo,
                            block_count));
}

// Verify that both BlockGroups with the BlockDuration before the Block
// and BlockGroups with the BlockDuration after the Block are supported
// correctly.
// Note: Raw bytes are use here because ClusterBuilder only generates
// one of these scenarios.
TEST_F(WebMClusterParserTest, ParseBlockGroup) {
  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, 23, false },
    { kVideoTrackNum, 33, 34, false },
  };
  int block_count = arraysize(kBlockInfo);

  const uint8 kClusterData[] = {
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
  const int kClusterSize = sizeof(kClusterData);

  int result = parser_->Parse(kClusterData, kClusterSize);
  EXPECT_EQ(result, kClusterSize);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseSimpleBlockAndBlockGroupMixture) {
  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, 23, true },
    { kAudioTrackNum, 23, 23, false },
    { kVideoTrackNum, 33, 34, true },
    { kAudioTrackNum, 46, 23, false },
    { kVideoTrackNum, 67, 33, false },
  };
  int block_count = arraysize(kBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, IgnoredTracks) {
  std::set<int64> ignored_tracks;
  ignored_tracks.insert(kTextTrackNum);

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kVideoTrackNum,
                                      WebMTracksParser::TextTracks(),
                                      ignored_tracks,
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kInputBlockInfo[] = {
    { kAudioTrackNum, 0,  23, true },
    { kAudioTrackNum, 23, 23, true },
    { kVideoTrackNum, 33, 33, true },
    { kTextTrackNum,  33, 99, true },
    { kAudioTrackNum, 46, 23, true },
    { kVideoTrackNum, 67, 33, true },
  };
  int input_block_count = arraysize(kInputBlockInfo);

  const BlockInfo kOutputBlockInfo[] = {
    { kAudioTrackNum, 0,  23, true },
    { kAudioTrackNum, 23, 23, true },
    { kVideoTrackNum, 33, 33, true },
    { kAudioTrackNum, 46, 23, true },
    { kVideoTrackNum, 67, 33, true },
  };
  int output_block_count = arraysize(kOutputBlockInfo);

  scoped_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kOutputBlockInfo, output_block_count));
}

TEST_F(WebMClusterParserTest, ParseTextTracks) {
  typedef WebMTracksParser::TextTracks TextTracks;
  TextTracks text_tracks;
  WebMTracksParser::TextTrackInfo text_track_info;

  text_track_info.kind = kTextSubtitles;
  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    text_track_info));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kVideoTrackNum,
                                      text_tracks,
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kInputBlockInfo[] = {
    { kAudioTrackNum, 0,  23, true },
    { kAudioTrackNum, 23, 23, true },
    { kVideoTrackNum, 33, 33, true },
    { kTextTrackNum,  33, 42, false },
    { kAudioTrackNum, 46, 23, true },
    { kTextTrackNum, 55, 44, false },
    { kVideoTrackNum, 67, 33, true },
  };
  int input_block_count = arraysize(kInputBlockInfo);

  scoped_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kInputBlockInfo, input_block_count));
}

TEST_F(WebMClusterParserTest, TextTracksSimpleBlock) {
  typedef WebMTracksParser::TextTracks TextTracks;
  TextTracks text_tracks;
  WebMTracksParser::TextTrackInfo text_track_info;

  text_track_info.kind = kTextSubtitles;
  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    text_track_info));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kVideoTrackNum,
                                      text_tracks,
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kInputBlockInfo[] = {
    { kTextTrackNum,  33, 42, true },
  };
  int input_block_count = arraysize(kInputBlockInfo);

  scoped_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_LT(result, 0);
}

TEST_F(WebMClusterParserTest, ParseMultipleTextTracks) {
  typedef WebMTracksParser::TextTracks TextTracks;
  TextTracks text_tracks;
  WebMTracksParser::TextTrackInfo text_track_info;

  const int kSubtitleTextTrackNum = kTextTrackNum;
  const int kCaptionTextTrackNum = kTextTrackNum + 1;

  text_track_info.kind = kTextSubtitles;
  text_tracks.insert(std::make_pair(TextTracks::key_type(kSubtitleTextTrackNum),
                                    text_track_info));

  text_track_info.kind = kTextCaptions;
  text_tracks.insert(std::make_pair(TextTracks::key_type(kCaptionTextTrackNum),
                                    text_track_info));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kVideoTrackNum,
                                      text_tracks,
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kInputBlockInfo[] = {
    { kAudioTrackNum, 0,  23, true },
    { kAudioTrackNum, 23, 23, true },
    { kVideoTrackNum, 33, 33, true },
    { kSubtitleTextTrackNum,  33, 42, false },
    { kAudioTrackNum, 46, 23, true },
    { kCaptionTextTrackNum, 55, 44, false },
    { kVideoTrackNum, 67, 33, true },
    { kSubtitleTextTrackNum,  67, 33, false },
  };
  int input_block_count = arraysize(kInputBlockInfo);

  scoped_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);

  WebMClusterParser::TextTrackIterator text_it =
      parser_->CreateTextTrackIterator();

  int text_track_num;
  const WebMClusterParser::BufferQueue* text_buffers;

  while (text_it(&text_track_num, &text_buffers)) {
    const WebMTracksParser::TextTracks::const_iterator find_result =
        text_tracks.find(text_track_num);
    ASSERT_TRUE(find_result != text_tracks.end());
    ASSERT_TRUE(VerifyTextBuffers(parser_, kInputBlockInfo, input_block_count,
                                  text_track_num, *text_buffers));
  }
}

TEST_F(WebMClusterParserTest, ParseEncryptedBlock) {
  scoped_ptr<Cluster> cluster(CreateEncryptedCluster(sizeof(kEncryptedFrame)));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kVideoTrackNum,
                                      WebMTracksParser::TextTracks(),
                                      std::set<int64>(),
                                      std::string(),
                                      "video_key_id",
                                      LogCB()));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_EQ(1UL, parser_->video_buffers().size());
  scoped_refptr<StreamParserBuffer> buffer = parser_->video_buffers()[0];
  EXPECT_TRUE(VerifyEncryptedBuffer(buffer));
}

TEST_F(WebMClusterParserTest, ParseBadEncryptedBlock) {
  scoped_ptr<Cluster> cluster(
      CreateEncryptedCluster(sizeof(kEncryptedFrame) - 1));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kVideoTrackNum,
                                      WebMTracksParser::TextTracks(),
                                      std::set<int64>(),
                                      std::string(),
                                      "video_key_id",
                                      LogCB()));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(-1, result);
}

}  // namespace media
