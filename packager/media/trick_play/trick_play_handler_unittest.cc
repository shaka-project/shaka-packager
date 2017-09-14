// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/trick_play/trick_play_handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/status_test_util.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex0 = 0;
const size_t kStreamIndex1 = 1;
const size_t kStreamIndex2 = 2;
const uint32_t kTimeScale = 800;
const uint32_t kDuration = 200;
const uint32_t kTrickPlayFactors[]{1, 2};
const uint32_t kTrickPlayFactorsDecreasing[]{2, 1};
const bool kEncrypted = true;
}  // namespace

MATCHER_P5(IsTrickPlayVideoStreamInfo,
           stream_index,
           time_scale,
           encrypted,
           trick_play_factor,
           playback_rate,
           "") {
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kStreamInfo &&
         arg->stream_info->time_scale() == time_scale &&
         arg->stream_info->is_encrypted() == encrypted &&
         arg->stream_info->stream_type() == kStreamVideo &&
         static_cast<const VideoStreamInfo*>(arg->stream_info.get())
                 ->trick_play_factor() == trick_play_factor &&
         static_cast<const VideoStreamInfo*>(arg->stream_info.get())
                 ->playback_rate() == playback_rate;
}

MATCHER_P3(IsKeyFrameMediaSample, stream_index, timestamp, duration, "") {
  return arg->stream_index == stream_index &&
         arg->stream_data_type == StreamDataType::kMediaSample &&
         arg->media_sample->dts() == timestamp &&
         arg->media_sample->duration() == duration &&
         arg->media_sample->is_key_frame() == true;
}

class TrickPlayHandlerTest : public MediaHandlerGraphTestBase {
 public:
  void SetUpTrickPlayHandler(const std::vector<uint32_t>& trick_play_factors) {
    trick_play_handler_.reset(new TrickPlayHandler());
    // Use SetUpGraph to set only input handler, and use
    // SetHandlerForMainStream and SetHandlerForTrickPlay for the output
    // handlers.
    SetUpGraph(1, 0, trick_play_handler_);
    trick_play_handler_->SetHandlerForMainStream(next_handler());
    for (uint32_t rate : trick_play_factors) {
      trick_play_handler_->SetHandlerForTrickPlay(rate, next_handler());
    }
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
  const std::vector<uint32_t> trick_play_factors(std::begin(kTrickPlayFactors),
                                                 std::end(kTrickPlayFactors));
  SetUpTrickPlayHandler(trick_play_factors);

  Status status = Process(StreamData::FromStreamInfo(
      kStreamIndex0, GetAudioStreamInfo(kTimeScale)));
  Status kExpectStatus(error::TRICK_PLAY_ERROR, "Some Messages");
  EXPECT_TRUE(status.Matches(kExpectStatus));
}

// This test makes sure the trick play handler can process stream data
// correctly.
TEST_F(TrickPlayHandlerTest, VideoStreamWithTrickPlay) {
  const std::vector<uint32_t> trick_play_factors(std::begin(kTrickPlayFactors),
                                                 std::end(kTrickPlayFactors));
  SetUpTrickPlayHandler(trick_play_factors);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex0, GetVideoStreamInfo(kTimeScale))));
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
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(
            kVideoStartTimestamp + kDuration * i,
            kDuration,
            is_key_frame))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 0, key frame.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp, kDuration,
                        !kEncrypted),
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
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(
            kVideoStartTimestamp + kDuration * i,
            kDuration,
            is_key_frame))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 3, key frame.
          IsKeyFrameMediaSample(
              kStreamIndex0, kVideoStartTimestamp + kDuration * 3, kDuration),
          // Stream info, TrickPlayFactor = 1.
          IsTrickPlayVideoStreamInfo(
              kStreamIndex1, kTimeScale, !kEncrypted, kTrickPlayFactors[0],
              static_cast<uint32_t>(kGOPSize) * kTrickPlayFactors[0]),
          // Frame 0, TrickPlayFactor = 1.
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
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(
            kVideoStartTimestamp + kDuration * i,
            kDuration,
            is_key_frame))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 6, key frame.
          IsKeyFrameMediaSample(
              kStreamIndex0, kVideoStartTimestamp + kDuration * 6, kDuration),
          // Frame 3, TrickPlayFactor = 1.
          IsKeyFrameMediaSample(kStreamIndex1,
                                kVideoStartTimestamp + kDuration * 3,
                                kDuration * 3),
          // Stream info, TrickPlayFactor = 2.
          IsTrickPlayVideoStreamInfo(
              kStreamIndex2, kTimeScale, !kEncrypted, kTrickPlayFactors[1],
              static_cast<uint32_t>(kGOPSize) * kTrickPlayFactors[1]),
          // Frame 0, TrickPlayFactor = 2.
          IsKeyFrameMediaSample(kStreamIndex2, kVideoStartTimestamp,
                                kDuration * 6),
          // Frame 7.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration * 7,
                        kDuration, !kEncrypted)));
  ClearOutputStreamDataVector();

  ASSERT_OK(FlushStream(0));
  EXPECT_THAT(GetOutputStreamDataVector(),
              ElementsAre(
                  // Frame 6, TrickPlayFactor = 1.
                  IsKeyFrameMediaSample(kStreamIndex1,
                                        kVideoStartTimestamp + kDuration * 6,
                                        kDuration * 2),
                  // Frame 6, TrickPlayFactor = 2.
                  IsKeyFrameMediaSample(kStreamIndex2,
                                        kVideoStartTimestamp + kDuration * 6,
                                        kDuration * 2)));
  ClearOutputStreamDataVector();

  // Flush again, get nothing.
  ASSERT_OK(FlushStream(0));
  EXPECT_THAT(GetOutputStreamDataVector(), IsEmpty());
}

// This test makes sure the trick play handler can process stream data
// correctly with a decreasing order of trick play factors.
TEST_F(TrickPlayHandlerTest, VideoStreamWithDecreasingTrickPlayFactors) {
  const std::vector<uint32_t> trick_play_factors(
      std::begin(kTrickPlayFactorsDecreasing),
      std::end(kTrickPlayFactorsDecreasing));
  SetUpTrickPlayHandler(trick_play_factors);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex0, GetVideoStreamInfo(kTimeScale))));
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
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(
            kVideoStartTimestamp + kDuration * i,
            kDuration,
            is_key_frame))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 0, key frame.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp, kDuration,
                        !kEncrypted),
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
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(
            kVideoStartTimestamp + kDuration * i,
            kDuration,
            is_key_frame))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 3, key frame.
          IsKeyFrameMediaSample(
              kStreamIndex0, kVideoStartTimestamp + kDuration * 3, kDuration),
          // Stream info, TrickPlayFactor = 1.
          IsTrickPlayVideoStreamInfo(
              kStreamIndex2, kTimeScale, !kEncrypted,
              kTrickPlayFactorsDecreasing[1],
              static_cast<uint32_t>(kGOPSize) * kTrickPlayFactorsDecreasing[1]),
          // Frame 0, TrickPlayFactor = 1.
          IsKeyFrameMediaSample(kStreamIndex2, kVideoStartTimestamp,
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
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(
            kVideoStartTimestamp + kDuration * i,
            kDuration,
            is_key_frame))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // Frame 6, key frame.
          IsKeyFrameMediaSample(
              kStreamIndex0, kVideoStartTimestamp + kDuration * 6, kDuration),
          // Stream info, TrickPlayFactor = 2.
          IsTrickPlayVideoStreamInfo(
              kStreamIndex1, kTimeScale, !kEncrypted,
              kTrickPlayFactorsDecreasing[0],
              static_cast<uint32_t>(kGOPSize) * kTrickPlayFactorsDecreasing[0]),
          // Frame 0, TrickPlayFactor = 2.
          IsKeyFrameMediaSample(kStreamIndex1, kVideoStartTimestamp,
                                kDuration * 6),
          // Frame 3, TrickPlayFactor = 1.
          IsKeyFrameMediaSample(kStreamIndex2,
                                kVideoStartTimestamp + kDuration * 3,
                                kDuration * 3),
          // Frame 7.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration * 7,
                        kDuration, !kEncrypted)));
  ClearOutputStreamDataVector();

  ASSERT_OK(FlushStream(0));
  EXPECT_THAT(GetOutputStreamDataVector(),
              ElementsAre(
                  // Frame 6, TrickPlayFactor = 2.
                  IsKeyFrameMediaSample(kStreamIndex1,
                                        kVideoStartTimestamp + kDuration * 6,
                                        kDuration * 2),
                  // Frame 6, TrickPlayFactor = 1.
                  IsKeyFrameMediaSample(kStreamIndex2,
                                        kVideoStartTimestamp + kDuration * 6,
                                        kDuration * 2)));
  ClearOutputStreamDataVector();

  // Flush again, get nothing.
  ASSERT_OK(FlushStream(0));
  EXPECT_THAT(GetOutputStreamDataVector(), IsEmpty());
}

}  // namespace media
}  // namespace shaka
