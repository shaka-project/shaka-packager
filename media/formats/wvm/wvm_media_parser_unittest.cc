// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_sample.h"
#include "media/base/stream_info.h"
#include "media/base/timestamp.h"
#include "media/base/video_stream_info.h"
#include "media/formats/wvm/wvm_media_parser.h"
#include "media/test/test_data_util.h"

namespace {
const char kClearWvmFile[] = "hb2_4stream_clear.wvm";
// Constants associated with kClearWvmFile follows.
const uint32 kExpectedStreams = 4;
const int kExpectedVideoFrameCount = 6665;
const int kExpectedAudioFrameCount = 11964;
}

namespace media {
namespace wvm {

class WvmMediaParserTest : public testing::Test {
 public:
  WvmMediaParserTest()
      : audio_frame_count_(0),
        video_frame_count_(0),
        video_max_dts_(kNoTimestamp),
        current_track_id_(-1) {
    parser_.reset(new WvmMediaParser());
  }

 protected:
  typedef std::map<int, scoped_refptr<StreamInfo> > StreamMap;

  scoped_ptr<WvmMediaParser> parser_;
  StreamMap stream_map_;
  int audio_frame_count_;
  int video_frame_count_;
  int64 video_max_dts_;
  uint32 current_track_id_;

  void OnInit(const std::vector<scoped_refptr<StreamInfo> >& stream_infos) {
    DVLOG(1) << "OnInit: " << stream_infos.size() << " streams.";
    for (std::vector<scoped_refptr<StreamInfo> >::const_iterator iter =
             stream_infos.begin(); iter != stream_infos.end(); ++iter) {
      DVLOG(1) << (*iter)->ToString();
      stream_map_[(*iter)->track_id()] = *iter;
    }
  }

  bool OnNewSample(uint32 track_id, const scoped_refptr<MediaSample>& sample) {
    std::string stream_type;
    if (track_id != current_track_id_) {
      // onto next track.
      video_max_dts_ = kNoTimestamp;
      current_track_id_ = track_id;
    }
    StreamMap::const_iterator stream = stream_map_.find(track_id);
    if (stream != stream_map_.end()) {
      if (stream->second->stream_type() == kStreamAudio) {
        ++audio_frame_count_;
        stream_type = "audio";
      } else if (stream->second->stream_type() == kStreamVideo) {
        ++video_frame_count_;
        stream_type = "video";
        // Verify timestamps are increasing.
        if (video_max_dts_ == kNoTimestamp) {
          video_max_dts_ = sample->dts();
        } else if (video_max_dts_ >= sample->dts()) {
          LOG(ERROR) << "Video DTS not strictly increasing for track = "
                     << track_id << ", video max dts = "
                     << video_max_dts_ << ", sample dts = "
                     << sample->dts();
          return false;
        }
        video_max_dts_ = sample->dts();
      } else {
        LOG(ERROR) << "Missing StreamInfo for track ID " << track_id;
        return false;
      }
    }

    return true;
  }

  void InitializeParser() {
    parser_->Init(
        base::Bind(&WvmMediaParserTest::OnInit,
                   base::Unretained(this)),
        base::Bind(&WvmMediaParserTest::OnNewSample,
                   base::Unretained(this)),
        NULL);
  }

  void Parse(const std::string& filename) {
    InitializeParser();

    std::vector<uint8> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(parser_->Parse(buffer.data(), buffer.size()));
  }
};

TEST_F(WvmMediaParserTest, ParseClear) {
  Parse(kClearWvmFile);
}

TEST_F(WvmMediaParserTest, StreamCount) {
  Parse(kClearWvmFile);
  EXPECT_EQ(kExpectedStreams, stream_map_.size());
}

TEST_F(WvmMediaParserTest, VideoFrameCount) {
  Parse(kClearWvmFile);
  EXPECT_EQ(kExpectedVideoFrameCount, video_frame_count_);
}

TEST_F(WvmMediaParserTest, AudioFrameCount) {
  Parse(kClearWvmFile);
  EXPECT_EQ(kExpectedAudioFrameCount, audio_frame_count_);
}

}  // namespace wvm
}  // namespace media
