// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/trick_play/trick_play_handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/base/test/status_test_util.h"
#include "packager/media/base/video_stream_info.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex0 = 0;
const size_t kStreamIndex1 = 1;
const size_t kStreamIndex2 = 2;
const size_t kStreamIndex3 = 3;
const uint32_t kTimeScale = 800;
const uint32_t kDuration = 200;
const int16_t kTrickPlayRates[]{1, 2, 4};
const bool kEncrypted = true;
}  // namespace

MATCHER_P4(IsTrickPlayVideoStreamInfo,
           stream_index,
           time_scale,
           encrypted,
           trick_play_rate,
           "") {
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kStreamInfo &&
         arg->stream_info->time_scale() == time_scale &&
         arg->stream_info->is_encrypted() == encrypted &&
         arg->stream_info->stream_type() == kStreamVideo &&
         static_cast<const VideoStreamInfo*>(arg->stream_info.get())
                 ->trick_play_rate() == trick_play_rate;
}

MATCHER_P3(IsKeyFrameMediaSample, stream_index, timestamp, duration, "") {
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kMediaSample &&
         arg->media_sample->dts() == timestamp &&
         arg->media_sample->duration() == duration &&
         arg->media_sample->is_key_frame() == true;
}

class TrickPlayHandlerTest : public MediaHandlerTestBase {
 public:
  void SetUpTrickPlayHandler(const TrickPlayOptions& trick_play_options) {
    trick_play_handler_.reset(new TrickPlayHandler(trick_play_options));
    // The output stream size is number of trick play stream + one
    // non-trick-play stream.
    SetUpGraph(1, trick_play_options.trick_play_rates.size() + 1,
               trick_play_handler_);
    ASSERT_OK(trick_play_handler_->Initialize());
  }

  Status Process(std::unique_ptr<StreamData> stream_data) {
    return trick_play_handler_->Process(std::move(stream_data));
  }

  Status FlushStream(size_t stream_index) {
    return trick_play_handler_->OnFlushRequest(stream_index);
  }

 protected:
  std::shared_ptr<TrickPlayHandler> trick_play_handler_;
};

// This test makes sure the audio stream is rejected by the trick play handler.
TEST_F(TrickPlayHandlerTest, AudioStream) {
  TrickPlayOptions trick_play_options;
  trick_play_options.trick_play_rates.assign(std::begin(kTrickPlayRates),
                                             std::end(kTrickPlayRates));
  SetUpTrickPlayHandler(trick_play_options);

  Status status =
      Process(GetAudioStreamInfoStreamData(kStreamIndex0, kTimeScale));
  Status kExpectStatus(error::TRICK_PLAY_ERROR, "Some Messages");
  EXPECT_TRUE(status.Matches(kExpectStatus));
}

// This test makes sure the trick play handler can process stream data
// correctly.
TEST_F(TrickPlayHandlerTest, VideoStream) {
  TrickPlayOptions trick_play_options;
  trick_play_options.trick_play_rates.assign(std::begin(kTrickPlayRates),
                                             std::end(kTrickPlayRates));
  SetUpTrickPlayHandler(trick_play_options);

  ASSERT_OK(Process(GetVideoStreamInfoStreamData(kStreamIndex0, kTimeScale)));
  // The stream info is cached, so the output is empty.
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex0, kTimeScale, !kEncrypted)));
  ClearOutputStreamDataVector();

  const int kVideoStartTimestamp = 12345;
  // Group of Picture size, the frequency of key frames.
  const int kGOPSize = 3;
  for (int i = 0; i < 3; ++i) {
    const bool is_key_frame = (i % kGOPSize == 0);
    ASSERT_OK(Process(GetMediaSampleStreamData(
        kStreamIndex0, kVideoStartTimestamp + kDuration * i, kDuration,
        is_key_frame)));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 0, key frame.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp, kDuration,
                        !kEncrypted),
          IsTrickPlayVideoStreamInfo(kStreamIndex1, kTimeScale, !kEncrypted,
                                     kTrickPlayRates[0]),
          IsTrickPlayVideoStreamInfo(kStreamIndex2, kTimeScale, !kEncrypted,
                                     kTrickPlayRates[1]),
          IsTrickPlayVideoStreamInfo(kStreamIndex3, kTimeScale, !kEncrypted,
                                     kTrickPlayRates[2]),
          // Frame 1.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration,
                        kDuration, !kEncrypted),
          // Frame 2.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration * 2,
                        kDuration, !kEncrypted)));
  ClearOutputStreamDataVector();

  // This expectation are separated from the expectation above because
  // ElementsAre supports at most 10 elements.
  for (int i = 3; i < 6; ++i) {
    const bool is_key_frame = (i % kGOPSize == 0);
    ASSERT_OK(Process(GetMediaSampleStreamData(
        kStreamIndex0, kVideoStartTimestamp + kDuration * i, kDuration,
        is_key_frame)));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 3, key frame.
          IsKeyFrameMediaSample(
              kStreamIndex0, kVideoStartTimestamp + kDuration * 3, kDuration),
          // Frame 0, TrickPlayRate = 1.
          IsKeyFrameMediaSample(kStreamIndex1, kVideoStartTimestamp,
                                kDuration * 3),
          // Frame 4.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration * 4,
                        kDuration, !kEncrypted),
          // Frame 5.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration * 5,
                        kDuration, !kEncrypted)));
  ClearOutputStreamDataVector();

  // This expectation are separated from the expectation above because
  // ElementsAre supports at most 10 elements.
  for (int i = 6; i < 8; ++i) {
    const bool is_key_frame = (i % kGOPSize == 0);
    ASSERT_OK(Process(GetMediaSampleStreamData(
        kStreamIndex0, kVideoStartTimestamp + kDuration * i, kDuration,
        is_key_frame)));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 6, key frame.
          IsKeyFrameMediaSample(
              kStreamIndex0, kVideoStartTimestamp + kDuration * 6, kDuration),
          // Frame 3, TrickPlayRate = 1.
          IsKeyFrameMediaSample(kStreamIndex1,
                                kVideoStartTimestamp + kDuration * 3,
                                kDuration * 3),
          // Frame 0, TrickPlayRate = 2.
          IsKeyFrameMediaSample(kStreamIndex2, kVideoStartTimestamp,
                                kDuration * 6),
          // Frame 7.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration * 7,
                        kDuration, !kEncrypted)));
  ClearOutputStreamDataVector();

  ASSERT_OK(FlushStream(0));
  EXPECT_THAT(GetOutputStreamDataVector(),
              ElementsAre(
                  // Frame 6, TrickPlayRate = 1.
                  IsKeyFrameMediaSample(kStreamIndex1,
                                        kVideoStartTimestamp + kDuration * 6,
                                        kDuration * 2),
                  // Frame 6, TrickPlayRate = 2.
                  IsKeyFrameMediaSample(kStreamIndex2,
                                        kVideoStartTimestamp + kDuration * 6,
                                        kDuration * 2),
                  // Frame 0, TrickPlayRate = 4.
                  IsKeyFrameMediaSample(kStreamIndex3, kVideoStartTimestamp,
                                        kDuration * 8)));
  ClearOutputStreamDataVector();

  // Flush again, get nothing.
  ASSERT_OK(FlushStream(0));
  EXPECT_THAT(GetOutputStreamDataVector(), IsEmpty());
}

}  // namespace media
}  // namespace shaka
