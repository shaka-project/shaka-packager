// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/trick_play/trick_play_handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;

// This value does not matter as trick play does not use it, but it is needed
// to create the audio and video info.
const uint32_t kTimescale = 1000u;

const bool kSubSegment = true;
const bool kEncrypted = true;
}  // namespace

MATCHER_P2(IsTrickPlayVideoStream, trick_play_factor, playback_rate, "") {
  if (arg->stream_data_type != StreamDataType::kStreamInfo ||
      arg->stream_info->stream_type() != kStreamVideo) {
    return false;
  }
  const VideoStreamInfo* video_info =
      static_cast<const VideoStreamInfo*>(arg->stream_info.get());
  return video_info->trick_play_factor() == trick_play_factor &&
         video_info->playback_rate() == playback_rate;
}

MATCHER_P2(IsTrickPlaySample, timestamp, duration, "") {
  return arg->stream_index == kStreamIndex &&
         arg->stream_data_type == StreamDataType::kMediaSample &&
         arg->media_sample->dts() == timestamp &&
         arg->media_sample->duration() == duration &&
         arg->media_sample->is_key_frame();
}

// Creates a structure of [input]->[trick play]->[output] where all
// interactions are with input and output.
class TrickPlayHandlerTest : public MediaHandlerTestBase {
 protected:
  void SetUpAndInitializeGraph(uint32_t factor) {
    input_ = std::make_shared<FakeInputMediaHandler>();
    trick_play_ = std::make_shared<TrickPlayHandler>(factor);
    output_ = std::make_shared<MockOutputMediaHandler>();

    ASSERT_OK(input_->AddHandler(trick_play_));
    ASSERT_OK(trick_play_->AddHandler(output_));

    ASSERT_OK(input_->Initialize());
  }

  FakeInputMediaHandler* input() { return input_.get(); }
  MockOutputMediaHandler* output() { return output_.get(); }

  // Create a series of samples where each sample has the same duration and ever
  // sample that is an even multiple of |key_frame_frequency| will be a key
  // frame.
  std::vector<std::shared_ptr<MediaSample>> CreateSamples(
      size_t count,
      uint64_t start_time,
      uint64_t frame_duration,
      size_t key_frame_frequency) {
    std::vector<std::shared_ptr<MediaSample>> samples;

    uint64_t time = start_time;

    for (size_t sample_index = 0; sample_index < count; sample_index++) {
      const bool is_key_frame = (sample_index % key_frame_frequency) == 0;
      samples.push_back(GetMediaSample(time, frame_duration, is_key_frame));
      time += frame_duration;
    }

    return samples;
  }

 private:
  std::shared_ptr<FakeInputMediaHandler> input_;
  std::shared_ptr<MediaHandler> trick_play_;
  std::shared_ptr<MockOutputMediaHandler> output_;
};

// This test makes sure that audio streams are rejected by trick play handlers.
TEST_F(TrickPlayHandlerTest, RejectsAudio) {
  const uint32_t kTrickPlayFactor = 1u;
  SetUpAndInitializeGraph(kTrickPlayFactor);

  Status status = input()->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetAudioStreamInfo(kTimescale)));
  EXPECT_EQ(error::TRICK_PLAY_ERROR, status.error_code());
}

// This test makes sure that when the trick play handler is initialized using
// a non-main-stream track that only a sub set of information gets passed
// through. This checks the specific case where no media samples are sent.
TEST_F(TrickPlayHandlerTest, TrickTrackNoSamples) {
  // When there are no samples, play rate could not be determined and should be
  // set to 0.
  const int64_t kPlayRate = 0;
  const uint32_t kTrickPlayFactor = 1u;

  SetUpAndInitializeGraph(kTrickPlayFactor);

  {
    testing::InSequence s;
    EXPECT_CALL(*output(),
                OnProcess(IsTrickPlayVideoStream(kTrickPlayFactor, kPlayRate)));
    EXPECT_CALL(*output(), OnFlush(kStreamIndex));
  }

  ASSERT_OK(input()->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetVideoStreamInfo(kTimescale))));
  ASSERT_OK(input()->FlushAllDownstreams());
}

// This test makes sure that when the trick play handler is initialized using
// a non-main-stream track that only a sub set of information gets passed
// through.
TEST_F(TrickPlayHandlerTest, TrickTrackWithSamplesOnlyGetsKeyFrames) {
  const uint32_t kTrickPlayFactor = 1u;
  const int64_t kStartTime = 0;
  const int64_t kDuration = 100;
  const int64_t kKeyFrameRate = 3;

  const int64_t kPlayRate = kKeyFrameRate * kTrickPlayFactor;
  const int64_t kTrickPlaySampleDuration = kDuration * kPlayRate;

  SetUpAndInitializeGraph(kTrickPlayFactor);

  // Only some samples we send to trick play should also be sent to our mock
  // handler.
  {
    testing::InSequence s;
    EXPECT_CALL(*output(),
                OnProcess(IsTrickPlayVideoStream(kTrickPlayFactor, kPlayRate)));
    for (int i = 0; i < 3; i++) {
      EXPECT_CALL(*output(), OnProcess(IsTrickPlaySample(
                                 kStartTime + i * kTrickPlaySampleDuration,
                                 kTrickPlaySampleDuration)));
    }
    EXPECT_CALL(*output(), OnFlush(kStreamIndex));
  }

  std::vector<std::shared_ptr<MediaSample>> samples =
      CreateSamples(9 /* sample count */, kStartTime, kDuration, kKeyFrameRate);

  ASSERT_OK(input()->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetVideoStreamInfo(kTimescale))));

  for (const auto& sample : samples) {
    ASSERT_OK(
        input()->Dispatch(StreamData::FromMediaSample(kStreamIndex, sample)));
  }

  ASSERT_OK(input()->FlushAllDownstreams());
}

// This test makes sure that when the trick play handler is initialized using
// a non-main-stream track that only a sub set of information gets passed
// through.
TEST_F(TrickPlayHandlerTest, TrickTrackWithSamples) {
  const uint32_t kTrickPlayFactor = 2u;
  const int64_t kStartTime = 0;
  const int64_t kDuration = 100;
  const int64_t kKeyFrameRate = 2;

  const int64_t kPlayRate = kKeyFrameRate * kTrickPlayFactor;
  const int64_t kTrickPlaySampleDuration = kDuration * kPlayRate;

  SetUpAndInitializeGraph(kTrickPlayFactor);

  // Only some samples we send to trick play should also be sent to our mock
  // handler.
  {
    testing::InSequence s;
    EXPECT_CALL(*output(),
                OnProcess(IsTrickPlayVideoStream(kTrickPlayFactor, kPlayRate)));
    for (int i = 0; i < 2; i++) {
      EXPECT_CALL(*output(), OnProcess(IsTrickPlaySample(
                                 kStartTime + i * kTrickPlaySampleDuration,
                                 kTrickPlaySampleDuration)));
    }
    EXPECT_CALL(*output(), OnFlush(0u));
  }

  std::vector<std::shared_ptr<MediaSample>> samples =
      CreateSamples(8 /* sample count */, kStartTime, kDuration, kKeyFrameRate);

  ASSERT_OK(input()->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetVideoStreamInfo(kTimescale))));

  for (const auto& sample : samples) {
    ASSERT_OK(
        input()->Dispatch(StreamData::FromMediaSample(kStreamIndex, sample)));
  }

  ASSERT_OK(input()->FlushAllDownstreams());
}

TEST_F(TrickPlayHandlerTest, TrickTrackWithSamplesAndSegments) {
  const uint32_t kTrickPlayFactor = 1u;
  const int64_t kStartTime = 0;
  const int64_t kDuration = 100;
  const int64_t kKeyFrameRate = 2;

  const int64_t kPlayRate = kKeyFrameRate * kTrickPlayFactor;
  const int64_t kTrickPlaySampleDuration = kDuration * kPlayRate;

  SetUpAndInitializeGraph(kTrickPlayFactor);

  // Only some samples we send to trick play should also be sent to our mock
  // handler.
  {
    testing::InSequence s;
    EXPECT_CALL(*output(),
                OnProcess(IsTrickPlayVideoStream(kTrickPlayFactor, kPlayRate)));

    // Segment One
    for (int i = 0; i < 2; i++) {
      EXPECT_CALL(*output(), OnProcess(IsTrickPlaySample(
                                 kStartTime + kTrickPlaySampleDuration * i,
                                 kTrickPlaySampleDuration)));
    }
    EXPECT_CALL(*output(),
                OnProcess(IsSegmentInfo(kStreamIndex, kStartTime, 4 * kDuration,
                                        !kSubSegment, !kEncrypted)));

    // Segment Two
    for (int i = 2; i < 4; i++) {
      EXPECT_CALL(*output(), OnProcess(IsTrickPlaySample(
                                 kStartTime + kTrickPlaySampleDuration * i,
                                 kTrickPlaySampleDuration)));
    }
    EXPECT_CALL(*output(), OnProcess(IsSegmentInfo(
                               kStreamIndex, kStartTime + 4 * kDuration,
                               4 * kDuration, !kSubSegment, !kEncrypted)));

    EXPECT_CALL(*output(), OnFlush(0u));
  }

  std::vector<std::shared_ptr<MediaSample>> segment_one_samples =
      CreateSamples(4 /* sample count */, kStartTime, kDuration, kKeyFrameRate);

  std::vector<std::shared_ptr<MediaSample>> segment_two_samples =
      CreateSamples(4 /* sample count */, kStartTime + 4 * kDuration, kDuration,
                    kKeyFrameRate);

  ASSERT_OK(input()->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetVideoStreamInfo(kTimescale))));

  // Segment One
  for (const auto& sample : segment_one_samples) {
    ASSERT_OK(
        input()->Dispatch(StreamData::FromMediaSample(kStreamIndex, sample)));
  }
  ASSERT_OK(input()->Dispatch(StreamData::FromSegmentInfo(
      kStreamIndex, GetSegmentInfo(kStartTime, kDuration * 4, !kEncrypted))));

  // Segment Two
  for (const auto& sample : segment_two_samples) {
    ASSERT_OK(
        input()->Dispatch(StreamData::FromMediaSample(kStreamIndex, sample)));
  }
  ASSERT_OK(input()->Dispatch(StreamData::FromSegmentInfo(
      kStreamIndex,
      GetSegmentInfo(kStartTime + 4 * kDuration, 4 * kDuration, !kEncrypted))));

  ASSERT_OK(input()->FlushAllDownstreams());
}

}  // namespace media
}  // namespace shaka
