// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp2t/mp2t_stream_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp2t {

class Mp2tStreamParserTest : public testing::Test {
 public:
  Mp2tStreamParserTest()
      : audio_frame_count_(0),
        video_frame_count_(0),
        video_min_dts_(kNoTimestamp()),
        video_max_dts_(kNoTimestamp()) {
    bool has_sbr = false;
    parser_.reset(new Mp2tStreamParser(has_sbr));
  }

 protected:
  scoped_ptr<Mp2tStreamParser> parser_;
  int audio_frame_count_;
  int video_frame_count_;
  base::TimeDelta video_min_dts_;
  base::TimeDelta video_max_dts_;

  bool AppendData(const uint8* data, size_t length) {
    return parser_->Parse(data, length);
  }

  bool AppendDataInPieces(const uint8* data, size_t length, size_t piece_size) {
    const uint8* start = data;
    const uint8* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void OnInit(bool init_ok,
              base::TimeDelta duration,
              bool auto_update_timestamp_offset) {
    DVLOG(1) << "OnInit: ok=" << init_ok
             << ", dur=" << duration.InMilliseconds()
             << ", autoTimestampOffset=" << auto_update_timestamp_offset;
  }

  bool OnNewConfig(const AudioDecoderConfig& ac,
                   const VideoDecoderConfig& vc,
                   const StreamParser::TextTrackConfigMap& tc) {
    DVLOG(1) << "OnNewConfig: audio=" << ac.IsValidConfig()
             << ", video=" << vc.IsValidConfig();
    return true;
  }


  void DumpBuffers(const std::string& label,
                   const StreamParser::BufferQueue& buffers) {
    DVLOG(2) << "DumpBuffers: " << label << " size " << buffers.size();
    for (StreamParser::BufferQueue::const_iterator buf = buffers.begin();
         buf != buffers.end(); buf++) {
      DVLOG(3) << "  n=" << buf - buffers.begin()
               << ", size=" << (*buf)->data_size()
               << ", dur=" << (*buf)->duration().InMilliseconds();
    }
  }

  bool OnNewBuffers(const StreamParser::BufferQueue& audio_buffers,
                    const StreamParser::BufferQueue& video_buffers,
                    const StreamParser::TextBufferQueueMap& text_map) {
    DumpBuffers("audio_buffers", audio_buffers);
    DumpBuffers("video_buffers", video_buffers);
    audio_frame_count_ += audio_buffers.size();
    video_frame_count_ += video_buffers.size();

    // TODO(wolenetz/acolwell): Add text track support to more MSE parsers. See
    // http://crbug.com/336926.
    if (!text_map.empty())
      return false;

    if (video_min_dts_ == kNoTimestamp() && !video_buffers.empty())
      video_min_dts_ = video_buffers.front()->GetDecodeTimestamp();
    if (!video_buffers.empty()) {
      video_max_dts_ = video_buffers.back()->GetDecodeTimestamp();
      // Verify monotonicity.
      StreamParser::BufferQueue::const_iterator it1 = video_buffers.begin();
      StreamParser::BufferQueue::const_iterator it2 = ++it1;
      for ( ; it2 != video_buffers.end(); ++it1, ++it2) {
        if ((*it2)->GetDecodeTimestamp() < (*it1)->GetDecodeTimestamp())
          return false;
      }
    }

    return true;
  }

  void OnKeyNeeded(const std::string& type,
                   const std::vector<uint8>& init_data) {
    DVLOG(1) << "OnKeyNeeded: " << init_data.size();
  }

  void OnNewSegment() {
    DVLOG(1) << "OnNewSegment";
  }

  void OnEndOfSegment() {
    DVLOG(1) << "OnEndOfSegment()";
  }

  void InitializeParser() {
    parser_->Init(
        base::Bind(&Mp2tStreamParserTest::OnInit,
                   base::Unretained(this)),
        base::Bind(&Mp2tStreamParserTest::OnNewConfig,
                   base::Unretained(this)),
        base::Bind(&Mp2tStreamParserTest::OnNewBuffers,
                   base::Unretained(this)),
        true,
        base::Bind(&Mp2tStreamParserTest::OnKeyNeeded,
                   base::Unretained(this)),
        base::Bind(&Mp2tStreamParserTest::OnNewSegment,
                   base::Unretained(this)),
        base::Bind(&Mp2tStreamParserTest::OnEndOfSegment,
                   base::Unretained(this)),
        LogCB());
  }

  bool ParseMpeg2TsFile(const std::string& filename, int append_bytes) {
    InitializeParser();

    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                   buffer->data_size(),
                                   append_bytes));
    return true;
  }
};

TEST_F(Mp2tStreamParserTest, UnalignedAppend17) {
  // Test small, non-segment-aligned appends.
  ParseMpeg2TsFile("bear-1280x720.ts", 17);
  EXPECT_EQ(video_frame_count_, 81);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
}

TEST_F(Mp2tStreamParserTest, UnalignedAppend512) {
  // Test small, non-segment-aligned appends.
  ParseMpeg2TsFile("bear-1280x720.ts", 512);
  EXPECT_EQ(video_frame_count_, 81);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
}

TEST_F(Mp2tStreamParserTest, TimestampWrapAround) {
  // "bear-1280x720_ptswraparound.ts" has been transcoded
  // from bear-1280x720.mp4 by applying a time offset of 95442s
  // (close to 2^33 / 90000) which results in timestamps wrap around
  // in the Mpeg2 TS stream.
  ParseMpeg2TsFile("bear-1280x720_ptswraparound.ts", 512);
  EXPECT_EQ(video_frame_count_, 81);
  EXPECT_GE(video_min_dts_, base::TimeDelta::FromSeconds(95443 - 10));
  EXPECT_LE(video_max_dts_, base::TimeDelta::FromSeconds(95443 + 10));
}

}  // namespace mp2t
}  // namespace media
