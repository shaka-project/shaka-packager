// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "packager/base/bind.h"
#include "packager/base/bind_helpers.h"
#include "packager/base/logging.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/formats/mp2t/mp2t_common.h"
#include "packager/media/formats/mp2t/mp2t_media_parser.h"
#include "packager/media/test/test_data_util.h"

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

  std::unique_ptr<Mp2tMediaParser> parser_;
  StreamMap stream_map_;
  int audio_frame_count_;
  int video_frame_count_;
  int64_t video_min_dts_;
  int64_t video_max_dts_;
  int64_t video_min_pts_;
  int64_t video_max_pts_;

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
    return false;
  }

  void InitializeParser() {
    parser_->Init(
        base::Bind(&Mp2tMediaParserTest::OnInit, base::Unretained(this)),
        base::Bind(&Mp2tMediaParserTest::OnNewSample, base::Unretained(this)),
        base::Bind(&Mp2tMediaParserTest::OnNewTextSample,
                   base::Unretained(this)),
        NULL);
  }

  bool ParseMpeg2TsFile(const std::string& filename, int append_bytes) {
    InitializeParser();

    std::vector<uint8_t> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer.data(),
                                   buffer.size(),
                                   append_bytes));
    return true;
  }
};

TEST_F(Mp2tMediaParserTest, UnalignedAppend17_H264) {
  // Test small, non-segment-aligned appends.
  ParseMpeg2TsFile("bear-640x360.ts", 17);
  EXPECT_EQ(79, video_frame_count_);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
}

TEST_F(Mp2tMediaParserTest, UnalignedAppend512_H264) {
  // Test small, non-segment-aligned appends.
  ParseMpeg2TsFile("bear-640x360.ts", 512);
  EXPECT_EQ(79, video_frame_count_);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
}

TEST_F(Mp2tMediaParserTest, UnalignedAppend17_H265) {
  // Test small, non-segment-aligned appends.
  ParseMpeg2TsFile("bear-640x360-hevc.ts", 17);
  EXPECT_EQ(78, video_frame_count_);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
}

TEST_F(Mp2tMediaParserTest, UnalignedAppend512_H265) {
  // Test small, non-segment-aligned appends.
  ParseMpeg2TsFile("bear-640x360-hevc.ts", 512);
  EXPECT_EQ(78, video_frame_count_);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
}

TEST_F(Mp2tMediaParserTest, TimestampWrapAround) {
  // "bear-640x360_ptszero_dtswraparound.ts" has been transcoded from
  // bear-640x360.mp4 by applying a time offset of 95442s (close to 2^33 /
  // 90000) which results in timestamp wrap around in the Mpeg2 TS stream.
  ParseMpeg2TsFile("bear-640x360_ptswraparound.ts", 512);
  EXPECT_TRUE(parser_->Flush());
  EXPECT_EQ(82, video_frame_count_);
  EXPECT_LT(video_min_dts_, static_cast<int64_t>(1) << 33);
  EXPECT_GT(video_max_dts_, static_cast<int64_t>(1) << 33);
}

TEST_F(Mp2tMediaParserTest, PtsZeroDtsWrapAround) {
  // "bear-640x360.ts" has been transcoded from bear-640x360.mp4 by applying a
  // dts (close to 2^33 / 90000) and pts 1433 which results in dts
  // wrap around in the Mpeg2 TS stream but pts does not.
  ParseMpeg2TsFile("bear-640x360_ptszero_dtswraparound.ts", 512);
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

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
