// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/muxer_listener_internal.h"

#include <gtest/gtest.h>

#include "packager/media/event/muxer_listener_test_helper.h"
#include "packager/mpd/base/media_info.pb.h"

namespace shaka {
namespace media {
namespace internal {
namespace {
const uint32_t kReferenceTimeScale = 1000;
}  // namespace

class MuxerListenerInternalTest : public ::testing::Test {};

class MuxerListenerInternalVideoStreamTest : public MuxerListenerInternalTest {
 protected:
  std::shared_ptr<VideoStreamInfo> video_stream_info_ =
      CreateVideoStreamInfo(GetDefaultVideoStreamInfoParams());
};

TEST_F(MuxerListenerInternalVideoStreamTest, Basic) {
  MediaInfo media_info;
  ASSERT_TRUE(GenerateMediaInfo(MuxerOptions(), *video_stream_info_,
                                kReferenceTimeScale,
                                MuxerListener::kContainerMp4, &media_info));
  ASSERT_TRUE(media_info.has_video_info());
  const MediaInfo_VideoInfo& video_info = media_info.video_info();
  EXPECT_EQ("avc1.010101", video_info.codec());
  EXPECT_EQ(720u, video_info.width());
  EXPECT_EQ(480u, video_info.height());
  EXPECT_EQ(10u, video_info.time_scale());
  EXPECT_EQ(1u, video_info.pixel_width());
  EXPECT_EQ(1u, video_info.pixel_height());
  EXPECT_EQ(0u, video_info.playback_rate());
  EXPECT_EQ(0u, video_info.transfer_characteristics());
}

TEST_F(MuxerListenerInternalVideoStreamTest, PixelWidthHeight) {
  MediaInfo media_info;
  video_stream_info_->set_pixel_width(100);
  video_stream_info_->set_pixel_height(200);
  ASSERT_TRUE(GenerateMediaInfo(MuxerOptions(), *video_stream_info_,
                                kReferenceTimeScale,
                                MuxerListener::kContainerMp4, &media_info));
  EXPECT_EQ(100u, media_info.video_info().pixel_width());
  EXPECT_EQ(200u, media_info.video_info().pixel_height());
}

TEST_F(MuxerListenerInternalVideoStreamTest, PlaybackRate) {
  MediaInfo media_info;
  video_stream_info_->set_playback_rate(5);
  ASSERT_TRUE(GenerateMediaInfo(MuxerOptions(), *video_stream_info_,
                                kReferenceTimeScale,
                                MuxerListener::kContainerMp4, &media_info));
  EXPECT_EQ(5u, media_info.video_info().playback_rate());
}

TEST_F(MuxerListenerInternalVideoStreamTest, TransferCharacteristics) {
  MediaInfo media_info;
  video_stream_info_->set_transfer_characteristics(18);
  ASSERT_TRUE(GenerateMediaInfo(MuxerOptions(), *video_stream_info_,
                                kReferenceTimeScale,
                                MuxerListener::kContainerMp4, &media_info));
  EXPECT_EQ(18u, media_info.video_info().transfer_characteristics());
}

}  // namespace internal
}  // namespace media
}  // namespace shaka
