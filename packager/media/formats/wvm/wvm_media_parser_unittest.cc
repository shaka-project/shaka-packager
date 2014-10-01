// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "packager/base/bind.h"
#include "packager/base/bind_helpers.h"
#include "packager/base/logging.h"
#include "packager/base/memory/ref_counted.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/request_signer.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/timestamp.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/base/widevine_key_source.h"
#include "packager/media/formats/wvm/wvm_media_parser.h"
#include "packager/media/test/test_data_util.h"

namespace {
const char kWvmFile[] = "hb2_4stream_encrypted.wvm";
// Constants associated with kWvmFile follows.
const uint32_t kExpectedStreams = 4;
const int kExpectedVideoFrameCount = 6665;
const int kExpectedAudioFrameCount = 11964;
}

namespace edash_packager {
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
    const std::string server_url =
        "https://license.uat.widevine.com/cenc/getcontentkey/widevine_test";
    const std::string aes_signing_key =
        "1ae8ccd0e7985cc0b6203a55855a1034afc252980e970ca90e5202689f947ab9";
    const std::string aes_signing_iv = "d58ce954203b7c9a9a9d467f59839249";
    const std::string signer = "widevine_test";
    request_signer_.reset(AesRequestSigner::CreateSigner(
        signer, aes_signing_key, aes_signing_iv));
    key_source_.reset(new WidevineKeySource(server_url,
                                            request_signer_.Pass()));
  }

 protected:
  typedef std::map<int, scoped_refptr<StreamInfo> > StreamMap;

  scoped_ptr<WvmMediaParser> parser_;
  scoped_ptr<RequestSigner> request_signer_;
  scoped_ptr<WidevineKeySource> key_source_;
  StreamMap stream_map_;
  int audio_frame_count_;
  int video_frame_count_;
  int64_t video_max_dts_;
  uint32_t current_track_id_;

  void OnInit(const std::vector<scoped_refptr<StreamInfo> >& stream_infos) {
    DVLOG(1) << "OnInit: " << stream_infos.size() << " streams.";
    for (std::vector<scoped_refptr<StreamInfo> >::const_iterator iter =
             stream_infos.begin(); iter != stream_infos.end(); ++iter) {
      DVLOG(1) << (*iter)->ToString();
      stream_map_[(*iter)->track_id()] = *iter;
    }
  }

  bool OnNewSample(uint32_t track_id,
                   const scoped_refptr<MediaSample>& sample) {
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
        key_source_.get());
  }

  void Parse(const std::string& filename) {
    InitializeParser();

    std::vector<uint8_t> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(parser_->Parse(buffer.data(), buffer.size()));
  }
};

TEST_F(WvmMediaParserTest, ParseWvm) {
  Parse(kWvmFile);
}

TEST_F(WvmMediaParserTest, StreamCount) {
  Parse(kWvmFile);
  EXPECT_EQ(kExpectedStreams, stream_map_.size());
}

TEST_F(WvmMediaParserTest, VideoFrameCount) {
  Parse(kWvmFile);
  EXPECT_EQ(kExpectedVideoFrameCount, video_frame_count_);
}

TEST_F(WvmMediaParserTest, AudioFrameCount) {
  Parse(kWvmFile);
  EXPECT_EQ(kExpectedAudioFrameCount, audio_frame_count_);
}

}  // namespace wvm
}  // namespace media
}  // namespace edash_packager
