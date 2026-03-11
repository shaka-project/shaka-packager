// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/mp2t/mp2t_media_parser.h>

#include <algorithm>
#include <functional>
#include <string>

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <packager/macros/logging.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/stream_info.h>
#include <packager/media/base/text_sample.h>
#include <packager/media/base/timestamp.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/formats/mp2t/mp2t_common.h>
#include <packager/media/test/test_data_util.h>

namespace shaka {
namespace media {
namespace mp2t {

class Mp2tMediaParserTest : public testing::Test {
 public:
  Mp2tMediaParserTest()
      : audio_frame_count_(0),
        video_frame_count_(0),
        video_min_dts_(kNoTimestamp),
        video_max_dts_(kNoTimestamp),
        video_min_pts_(kNoTimestamp),
        video_max_pts_(kNoTimestamp) {
    parser_.reset(new Mp2tMediaParser());
  }

 protected:
  typedef std::map<int, std::shared_ptr<StreamInfo>> StreamMap;

  struct TextSampleInfo {
    uint32_t track_id;
    int64_t start_time;
    int64_t end_time;
    TextSampleRole role;
  };

  std::unique_ptr<Mp2tMediaParser> parser_;
  StreamMap stream_map_;
  int audio_frame_count_;
  int video_frame_count_;
  int64_t video_min_dts_;
  int64_t video_max_dts_;
  int64_t video_min_pts_;
  int64_t video_max_pts_;
  std::vector<TextSampleInfo> text_samples_;

  bool AppendData(const uint8_t* data, size_t length) {
    return parser_->Parse(data, static_cast<int>(length));
  }

  bool AppendDataInPieces(const uint8_t* data,
                          size_t length,
                          size_t piece_size) {
    const uint8_t* start = data;
    const uint8_t* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void OnInit(const std::vector<std::shared_ptr<StreamInfo>>& stream_infos) {
    DVLOG(1) << "OnInit: " << stream_infos.size() << " streams.";
    for (const auto& stream_info : stream_infos) {
      DVLOG(1) << stream_info->ToString();
      stream_map_[stream_info->track_id()] = stream_info;
    }
  }

  bool OnNewSample(uint32_t track_id, std::shared_ptr<MediaSample> sample) {
    StreamMap::const_iterator stream = stream_map_.find(track_id);
    EXPECT_NE(stream_map_.end(), stream);
    if (stream != stream_map_.end()) {
      if (stream->second->stream_type() == kStreamAudio) {
        ++audio_frame_count_;
      } else if (stream->second->stream_type() == kStreamVideo) {
        ++video_frame_count_;
        if (video_min_dts_ == kNoTimestamp)
          video_min_dts_ = sample->dts();
        if (video_min_pts_ == kNoTimestamp || video_min_pts_ > sample->pts())
          video_min_pts_ = sample->pts();
        // Verify timestamps are increasing.
        if (video_max_dts_ == kNoTimestamp)
          video_max_dts_ = sample->dts();
        else if (video_max_dts_ >= sample->dts()) {
          LOG(ERROR) << "Video DTS not strictly increasing.";
          return false;
        }
        if (video_max_pts_ < sample->pts()) {
          video_max_pts_ = sample->pts();
        }
        video_max_dts_ = sample->dts();
      } else {
        LOG(ERROR) << "Missing StreamInfo for track ID " << track_id;
        return false;
      }
    }

    return true;
  }

  bool OnNewTextSample(uint32_t track_id, std::shared_ptr<TextSample> sample) {
    text_samples_.push_back(
        {track_id, sample->start_time(), sample->EndTime(), sample->role()});
    return true;
  }

  void InitializeParser() {
    parser_->Init(
        std::bind(&Mp2tMediaParserTest::OnInit, this, std::placeholders::_1),
        std::bind(&Mp2tMediaParserTest::OnNewSample, this,
                  std::placeholders::_1, std::placeholders::_2),
        std::bind(&Mp2tMediaParserTest::OnNewTextSample, this,
                  std::placeholders::_1, std::placeholders::_2),
        NULL);
  }

  bool ParseMpeg2TsFile(const std::string& filename, int append_bytes) {
    InitializeParser();

    std::vector<uint8_t> buffer = ReadTestDataFile(filename);
    if (buffer.empty())
      return false;

    return AppendDataInPieces(buffer.data(), buffer.size(), append_bytes);
  }
};

TEST_F(Mp2tMediaParserTest, UnalignedAppend17_H264) {
  // Test small, non-segment-aligned appends.
  ASSERT_TRUE(ParseMpeg2TsFile("bear-640x360.ts", 17));
  EXPECT_EQ(79, video_frame_count_);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
}

TEST_F(Mp2tMediaParserTest, UnalignedAppend512_H264) {
  // Test small, non-segment-aligned appends.
  ASSERT_TRUE(ParseMpeg2TsFile("bear-640x360.ts", 512));
  EXPECT_EQ(79, video_frame_count_);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
}

TEST_F(Mp2tMediaParserTest, UnalignedAppend17_H265) {
  // Test small, non-segment-aligned appends.
  ASSERT_TRUE(ParseMpeg2TsFile("bear-640x360-hevc.ts", 17));
  EXPECT_EQ(78, video_frame_count_);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
}

TEST_F(Mp2tMediaParserTest, UnalignedAppend512_H265) {
  // Test small, non-segment-aligned appends.
  ASSERT_TRUE(ParseMpeg2TsFile("bear-640x360-hevc.ts", 512));
  EXPECT_EQ(78, video_frame_count_);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
}

TEST_F(Mp2tMediaParserTest, TimestampWrapAround) {
  // "bear-640x360_ptszero_dtswraparound.ts" has been transcoded from
  // bear-640x360.mp4 by applying a time offset of 95442s (close to 2^33 /
  // 90000) which results in timestamp wrap around in the Mpeg2 TS stream.
  ASSERT_TRUE(ParseMpeg2TsFile("bear-640x360_ptswraparound.ts", 512));
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
  EXPECT_LT(video_min_dts_, static_cast<int64_t>(1) << 33);
  EXPECT_GT(video_max_dts_, static_cast<int64_t>(1) << 33);
}

TEST_F(Mp2tMediaParserTest, PtsZeroDtsWrapAround) {
  // "bear-640x360.ts" has been transcoded from bear-640x360.mp4 by applying a
  // dts (close to 2^33 / 90000) and pts 1433 which results in dts
  // wrap around in the Mpeg2 TS stream but pts does not.
  ASSERT_TRUE(ParseMpeg2TsFile("bear-640x360_ptszero_dtswraparound.ts", 512));
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(64, video_frame_count_);
  // DTS was subjected to unroll
  EXPECT_LT(video_min_dts_, static_cast<int64_t>(1) << 33);
  EXPECT_GT(video_max_dts_, static_cast<int64_t>(1) << 33);
  // PTS was not subjected to unroll but was artificially unrolled to be close
  // to DTS
  EXPECT_GT(video_min_pts_, static_cast<int64_t>(1) << 33);
  EXPECT_GT(video_max_pts_, static_cast<int64_t>(1) << 33);
}

TEST_F(Mp2tMediaParserTest, PmtEsDescriptors) {
  //"bear-eng-visualy-impaired-audio.ts" consist of audio stream marked as
  // english audio with commentary for visualy impaired viewer and max
  // bitrate set to ~128kbps

  ParseMpeg2TsFile("bear-visualy-impaired-eng-audio.ts", 188);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_STREQ("eng", stream_map_[257]->language().c_str());

  auto* audio_info = static_cast<AudioStreamInfo*>(stream_map_[257].get());
  EXPECT_EQ(131600, audio_info->max_bitrate());
}

TEST_F(Mp2tMediaParserTest, TeletextHeartbeatGeneration) {
  // Test that MediaHeartBeat samples are generated during parsing of a
  // teletext TS file with sparse subtitle content.
  //
  // test_teletext_live.ts contains:
  // - ~25 seconds of video/audio
  // - 3 teletext cues with gaps:
  //   - Cue 1: 1.0-3.0s (PTS 90000-270000)
  //   - Cue 2: 3.5-4.5s (PTS 315000-405000)
  //   - Cue 3: 13.0-21.0s (PTS 1170000-1890000)
  //
  // Expected behavior:
  // - MediaHeartBeat samples generated every 100ms (9000 ticks) from video PTS
  // - kCueStart/kCueEnd samples for each subtitle cue
  // - Samples arrive during Parse(), not just Flush()

  ASSERT_TRUE(ParseMpeg2TsFile("test_teletext_live.ts", 188));
  EXPECT_TRUE(parser_->Flush());

  // Separate samples by role
  int heartbeat_count = 0;
  int cue_start_count = 0;
  int cue_end_count = 0;
  int text_heartbeat_count = 0;

  std::vector<int64_t> heartbeat_pts_list;

  for (const auto& sample : text_samples_) {
    switch (sample.role) {
      case TextSampleRole::kMediaHeartBeat:
        heartbeat_count++;
        heartbeat_pts_list.push_back(sample.start_time);
        break;
      case TextSampleRole::kCueStart:
        cue_start_count++;
        break;
      case TextSampleRole::kCueEnd:
        cue_end_count++;
        break;
      case TextSampleRole::kTextHeartBeat:
        text_heartbeat_count++;
        break;
      default:
        break;
    }
  }

  // Verify MediaHeartBeat samples were generated
  // ~25 seconds of video at 100ms intervals = ~250 heartbeats
  // Allow some tolerance for actual file duration
  EXPECT_GT(heartbeat_count, 200)
      << "Expected at least 200 MediaHeartBeat samples for ~25s video";
  EXPECT_LT(heartbeat_count, 300)
      << "Expected less than 300 MediaHeartBeat samples";

  // Verify heartbeat timing: should be at ~100ms (9000 ticks) intervals
  if (heartbeat_pts_list.size() >= 2) {
    // Check first few intervals
    size_t intervals_to_check =
        std::min(size_t(10), heartbeat_pts_list.size() - 1);
    for (size_t i = 0; i < intervals_to_check; ++i) {
      int64_t interval = heartbeat_pts_list[i + 1] - heartbeat_pts_list[i];
      // Expect ~9000 ticks (100ms), allow some tolerance
      EXPECT_GE(interval, 9000) << "Heartbeat interval at index " << i;
      EXPECT_LE(interval, 18000) << "Heartbeat interval at index " << i
                                 << " (should be ~9000, max ~18000 for 200ms)";
    }
  }

  // Verify subtitle cue samples
  // test_teletext_live.ts has 3 cues, each with START and END PES packets:
  // - 3 kCueStart samples (one per cue, from START PES with text)
  // - 6 kCueEnd samples (START PES with erase_bit=1 + END PES with erase_bit=1)
  // Every PES packet starts with packet 0 (erase_bit=1), which triggers kCueEnd
  EXPECT_EQ(cue_start_count, 3)
      << "Expected 3 kCueStart samples for 3 subtitle cues";
  EXPECT_EQ(cue_end_count, 6) << "Expected 6 kCueEnd samples: 3 cues × 2 PES "
                                 "(start+end) with erase_bit=1";

  // TextHeartBeat samples are optional (emitted when PES arrives with no
  // content) We don't enforce a specific count, just log for debugging
  LOG(INFO) << "TextHeartBeat count: " << text_heartbeat_count;

  // Verify total sample count is reasonable
  EXPECT_GT(text_samples_.size(), heartbeat_count)
      << "Total text samples should include heartbeats + cue samples";

  // Log summary for debugging
  LOG(INFO) << "Teletext heartbeat test summary:";
  LOG(INFO) << "  Total text samples: " << text_samples_.size();
  LOG(INFO) << "  MediaHeartBeat: " << heartbeat_count;
  LOG(INFO) << "  kCueStart: " << cue_start_count;
  LOG(INFO) << "  kCueEnd: " << cue_end_count;
  LOG(INFO) << "  TextHeartBeat: " << text_heartbeat_count;
}

TEST_F(Mp2tMediaParserTest, TeletextPtsWrapAround) {
  // Test that the parser correctly handles PTS values near the 33-bit
  // wrap-around point (2^33 = 8589934592 ticks, ~26.5 hours at 90kHz).
  //
  // test_teletext_live_wrap.ts contains:
  // - Video/audio/teletext with PTS starting ~15 seconds before wrap
  // - Content that spans across the wrap boundary
  // - Same structure as test_teletext_live.ts but with offset timestamps
  //
  // Expected behavior:
  // - Parser correctly unwraps PTS values to monotonically increasing
  // timestamps
  // - MediaHeartBeat samples generated with correct unwrapped timestamps
  // - kCueStart/kCueEnd samples have correct timing relative to wrap

  ASSERT_TRUE(ParseMpeg2TsFile("test_teletext_live_wrap.ts", 188));
  EXPECT_TRUE(parser_->Flush());

  // Verify we got samples
  EXPECT_FALSE(text_samples_.empty()) << "Expected text samples from teletext";

  // Separate samples by role and collect timestamps
  int heartbeat_count = 0;
  int cue_start_count = 0;
  int cue_end_count = 0;

  std::vector<int64_t> all_pts_values;

  for (const auto& sample : text_samples_) {
    all_pts_values.push_back(sample.start_time);

    switch (sample.role) {
      case TextSampleRole::kMediaHeartBeat:
        heartbeat_count++;
        break;
      case TextSampleRole::kCueStart:
        cue_start_count++;
        break;
      case TextSampleRole::kCueEnd:
        cue_end_count++;
        break;
      default:
        break;
    }
  }

  // Verify MediaHeartBeat samples were generated
  EXPECT_GT(heartbeat_count, 100)
      << "Expected MediaHeartBeat samples for wrap-around test";

  // Verify subtitle cue samples (same structure as test_teletext_live.ts)
  EXPECT_EQ(cue_start_count, 3)
      << "Expected 3 kCueStart samples for 3 subtitle cues";
  EXPECT_EQ(cue_end_count, 6) << "Expected 6 kCueEnd samples";

  // Key verification: Check that PTS values are near the wrap-around point
  // The 33-bit wrap point is 8589934592
  constexpr int64_t kPtsWrapAround = 1LL << 33;  // 8589934592

  // Find min and max PTS to verify we're near wrap point
  int64_t min_pts =
      *std::min_element(all_pts_values.begin(), all_pts_values.end());
  int64_t max_pts =
      *std::max_element(all_pts_values.begin(), all_pts_values.end());

  // Verify timestamps are near the wrap point (within ~30 seconds = 2.7M ticks)
  constexpr int64_t kMaxDistanceFromWrap = 30 * 90000;  // 30 seconds
  EXPECT_GT(min_pts, kPtsWrapAround - kMaxDistanceFromWrap)
      << "Expected min PTS to be within 30s before wrap point";

  // After unwrapping, max PTS should be greater than wrap point
  // (content spans across the wrap boundary)
  EXPECT_GT(max_pts, kPtsWrapAround)
      << "Expected max PTS to be after wrap point (content spans wrap)";

  // Verify monotonicity: all timestamps should be increasing (after unwrapping)
  // This confirms the parser correctly handles the wrap-around
  bool is_monotonic = true;
  for (size_t i = 1; i < all_pts_values.size(); ++i) {
    if (all_pts_values[i] < all_pts_values[i - 1]) {
      // Allow small backwards jumps (up to 1 second) due to B-frames or
      // interleaved streams, but not wrap-around sized jumps
      int64_t diff = all_pts_values[i - 1] - all_pts_values[i];
      if (diff > 90000) {  // More than 1 second backwards
        is_monotonic = false;
        LOG(ERROR) << "Non-monotonic PTS at index " << i << ": "
                   << all_pts_values[i - 1] << " -> " << all_pts_values[i]
                   << " (diff: " << diff << ")";
        break;
      }
    }
  }
  EXPECT_TRUE(is_monotonic)
      << "PTS values should be monotonically increasing after unwrapping";

  // Log summary for debugging
  LOG(INFO) << "Teletext wrap-around test summary:";
  LOG(INFO) << "  Total text samples: " << text_samples_.size();
  LOG(INFO) << "  MediaHeartBeat: " << heartbeat_count;
  LOG(INFO) << "  kCueStart: " << cue_start_count;
  LOG(INFO) << "  kCueEnd: " << cue_end_count;
  LOG(INFO) << "  Min PTS: " << min_pts << " (wrap point: " << kPtsWrapAround
            << ")";
  LOG(INFO) << "  Max PTS: " << max_pts;
  LOG(INFO) << "  Distance from wrap: " << (kPtsWrapAround - min_pts) / 90000
            << "s before, " << (max_pts - kPtsWrapAround) / 90000 << "s after";
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
