// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/chunking/cue_alignment_handler.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/ad_cue_generator_params.h>
#include <packager/macros/status.h>
#include <packager/media/base/media_handler_test_base.h>
#include <packager/media/chunking/chunking_handler.h>
#include <packager/status/status_test_util.h>

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace shaka {
namespace media {
namespace {
const size_t kOneInput = 1;
const size_t kOneOutput = 1;

const size_t kThreeInputs = 3;
const size_t kThreeOutputs = 3;

const bool kKeyFrame = true;

const int32_t kMsTimeScale = 1000;

const size_t kStreamIndex = 0;
}  // namespace

class CueAlignmentHandlerTest : public MediaHandlerTestBase {
 protected:
  std::unique_ptr<SyncPointQueue> CreateSyncPoints(
      std::initializer_list<double> cues) {
    AdCueGeneratorParams params;

    for (double cue_time : cues) {
      Cuepoint cue;
      cue.start_time_in_seconds = cue_time;

      params.cue_points.push_back(cue);
    }

    return std::unique_ptr<SyncPointQueue>(new SyncPointQueue(params));
  }

  Status DispatchAudioInfo(size_t input_index) {
    auto info = GetAudioStreamInfo(kMsTimeScale);
    auto data = StreamData::FromStreamInfo(kStreamIndex, std::move(info));

    return Input(input_index)->Dispatch(std::move(data));
  }

  Status DispatchTextInfo(size_t input_index) {
    auto info = GetTextStreamInfo(kMsTimeScale);
    auto data = StreamData::FromStreamInfo(kStreamIndex, std::move(info));

    return Input(input_index)->Dispatch(std::move(data));
  }

  Status DispatchVideoInfo(size_t input_index) {
    auto info = GetVideoStreamInfo(kMsTimeScale);
    auto data = StreamData::FromStreamInfo(kStreamIndex, std::move(info));

    return Input(input_index)->Dispatch(std::move(data));
  }

  Status DispatchMediaSample(size_t input_index,
                             int64_t start_time,
                             int64_t duration,
                             bool keyframe) {
    auto sample = GetMediaSample(start_time, duration, keyframe);
    auto data = StreamData::FromMediaSample(kStreamIndex, std::move(sample));

    return Input(input_index)->Dispatch(std::move(data));
  }

  Status DispatchTextSample(size_t input_index,
                            int64_t start_time,
                            int64_t end_time) {
    const char* kNoId = "";
    const char* kNoPayload = "";

    auto sample = GetTextSample(kNoId, start_time, end_time, kNoPayload);
    auto data = StreamData::FromTextSample(kStreamIndex, std::move(sample));

    return Input(input_index)->Dispatch(std::move(data));
  }

  Status FlushAll(std::initializer_list<size_t> inputs) {
    for (auto& input : inputs) {
      RETURN_IF_ERROR(Input(input)->FlushAllDownstreams());
    }

    return Status::OK;
  }
};

TEST_F(CueAlignmentHandlerTest, VideoInputWithNoCues) {
  const size_t kVideoStream = 0;

  const int64_t kSampleDuration = 1000;
  const int64_t kSample0Start = 0;
  const int64_t kSample1Start = kSample0Start + kSampleDuration;
  const int64_t kSample2Start = kSample1Start + kSampleDuration;

  auto sync_points = CreateSyncPoints({});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample0Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample1Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample2Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kVideoStream), OnFlush(_));
  }

  ASSERT_OK(DispatchVideoInfo(kVideoStream));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample0Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample1Start, kSampleDuration,
                                !kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample2Start, kSampleDuration,
                                kKeyFrame));

  ASSERT_OK(FlushAll({kVideoStream}));
}

TEST_F(CueAlignmentHandlerTest, AudioInputWithNoCues) {
  const size_t kAudioStream = 0;

  const int64_t kSampleDuration = 1000;
  const int64_t kSample0Start = 0;
  const int64_t kSample1Start = kSample0Start + kSampleDuration;
  const int64_t kSample2Start = kSample1Start + kSampleDuration;

  auto sync_points = CreateSyncPoints({});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample0Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample1Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample2Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kAudioStream), OnFlush(_));
  }

  ASSERT_OK(DispatchAudioInfo(kAudioStream));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample0Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample1Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample2Start, kSampleDuration,
                                kKeyFrame));

  ASSERT_OK(FlushAll({kAudioStream}));
}

TEST_F(CueAlignmentHandlerTest, TextInputWithNoCues) {
  const size_t kTextStream = 0;

  const int64_t kSampleDuration = 1000;

  const int64_t kSample0Start = 0;
  const int64_t kSample0End = kSample0Start + kSampleDuration;
  const int64_t kSample1Start = kSample0End;
  const int64_t kSample1End = kSample1Start + kSampleDuration;
  const int64_t kSample2Start = kSample1End;
  const int64_t kSample2End = kSample2Start + kSampleDuration;

  auto sync_points = CreateSyncPoints({});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample0Start, kSample0End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample1Start, kSample1End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample2Start, kSample2End)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(_));
  }

  ASSERT_OK(DispatchTextInfo(kTextStream));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample0Start, kSample0End));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample1Start, kSample1End));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample2Start, kSample2End));

  ASSERT_OK(FlushAll({kTextStream}));
}

TEST_F(CueAlignmentHandlerTest, TextAudioVideoInputWithNoCues) {
  const size_t kTextStream = 0;
  const size_t kAudioStream = 1;
  const size_t kVideoStream = 2;

  const int64_t kSampleDuration = 1000;

  const int64_t kSample0Start = 0;
  const int64_t kSample0End = kSample0Start + kSampleDuration;
  const int64_t kSample1Start = kSample0Start + kSampleDuration;
  const int64_t kSample1End = kSample1Start + kSampleDuration;
  const int64_t kSample2Start = kSample1Start + kSampleDuration;
  const int64_t kSample2End = kSample2Start + kSampleDuration;

  auto sync_points = CreateSyncPoints({});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kThreeInputs, kThreeOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample0Start, kSample0End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample1Start, kSample1End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample2Start, kSample2End)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(_));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample0Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample1Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample2Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kAudioStream), OnFlush(_));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample0Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample1Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample2Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kVideoStream), OnFlush(_));
  }

  // Dispatch Stream Info
  ASSERT_OK(DispatchTextInfo(kTextStream));
  ASSERT_OK(DispatchAudioInfo(kAudioStream));
  ASSERT_OK(DispatchVideoInfo(kVideoStream));

  // Dispatch Sample 0
  ASSERT_OK(DispatchTextSample(kTextStream, kSample0Start, kSample0End));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample0Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample0Start, kSampleDuration,
                                kKeyFrame));

  // Dispatch Sample 1
  ASSERT_OK(DispatchTextSample(kTextStream, kSample1Start, kSample1End));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample1Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample1Start, kSampleDuration,
                                !kKeyFrame));

  // Dispatch Sample 2
  ASSERT_OK(DispatchTextSample(kTextStream, kSample2Start, kSample2End));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample2Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample2Start, kSampleDuration,
                                kKeyFrame));

  ASSERT_OK(FlushAll({kTextStream, kAudioStream, kVideoStream}));
}

TEST_F(CueAlignmentHandlerTest, VideoInputWithCues) {
  const size_t kVideoStream = 0;

  const int64_t kSampleDuration = 1000;
  const int64_t kSample0Start = 0;
  const int64_t kSample1Start = kSample0Start + kSampleDuration;
  const int64_t kSample2Start = kSample1Start + kSampleDuration;

  const double kSample2StartInSeconds =
      static_cast<double>(kSample2Start) / kMsTimeScale;

  // Put the cue between two key frames.
  auto sync_points =
      CreateSyncPoints({static_cast<double>(kSample1Start) / kMsTimeScale});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample0Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample1Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsCueEvent(_, kSample2StartInSeconds)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample2Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kVideoStream), OnFlush(_));
  }

  ASSERT_OK(DispatchVideoInfo(kVideoStream));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample0Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample1Start, kSampleDuration,
                                !kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample2Start, kSampleDuration,
                                kKeyFrame));

  ASSERT_OK(FlushAll({kVideoStream}));
}

TEST_F(CueAlignmentHandlerTest, AudioInputWithCues) {
  const size_t kAudioStream = 0;

  const int64_t kSampleDuration = 1000;
  const int64_t kSample0Start = 0;
  const int64_t kSample1Start = kSample0Start + kSampleDuration;
  const int64_t kSample2Start = kSample1Start + kSampleDuration;

  const double kSample1StartInSeconds =
      static_cast<double>(kSample1Start) / kMsTimeScale;

  auto sync_points =
      CreateSyncPoints({static_cast<double>(kSample1Start) / kMsTimeScale});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample0Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsCueEvent(_, kSample1StartInSeconds)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample1Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample2Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kAudioStream), OnFlush(_));
  }

  ASSERT_OK(DispatchAudioInfo(kAudioStream));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample0Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample1Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample2Start, kSampleDuration,
                                kKeyFrame));

  ASSERT_OK(FlushAll({kAudioStream}));
}

TEST_F(CueAlignmentHandlerTest, TextInputWithCues) {
  const size_t kTextStream = 0;

  const int64_t kSampleDuration = 1000;

  const int64_t kSample0Start = 0;
  const int64_t kSample0End = kSample0Start + kSampleDuration;
  const int64_t kSample1Start = kSample0End;
  const int64_t kSample1End = kSample1Start + kSampleDuration;
  const int64_t kSample2Start = kSample1End;
  const int64_t kSample2End = kSample2Start + kSampleDuration;

  const double kSample1StartInSeconds =
      static_cast<double>(kSample1Start) / kMsTimeScale;

  auto sync_points =
      CreateSyncPoints({static_cast<double>(kSample1Start) / kMsTimeScale});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample0Start, kSample0End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsCueEvent(_, kSample1StartInSeconds)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample1Start, kSample1End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample2Start, kSample2End)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(_));
  }

  ASSERT_OK(DispatchTextInfo(kTextStream));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample0Start, kSample0End));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample1Start, kSample1End));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample2Start, kSample2End));

  ASSERT_OK(FlushAll({kTextStream}));
}

TEST_F(CueAlignmentHandlerTest, TextInputWithCueAfterLastStart) {
  const size_t kTextStream = 0;

  const int64_t kSampleDuration = 1000;

  const int64_t kSample0Start = 0;
  const int64_t kSample0End = kSample0Start + kSampleDuration;
  const int64_t kSample1Start = kSample0End;
  const int64_t kSample1End = kSample1Start + kSampleDuration;
  const int64_t kSample2Start = kSample1End;
  const int64_t kSample2End = kSample2Start + kSampleDuration;

  // Put the cue 1 between the start and end of the last sample.
  const double kCue1TimeInSeconds =
      static_cast<double>(kSample2Start + kSample2End) / 2 / kMsTimeScale;

  // Put the cue 2 after the end of the last sample.
  const double kCue2TimeInSeconds =
      static_cast<double>(kSample2End) / kMsTimeScale;

  auto sync_points = CreateSyncPoints({kCue1TimeInSeconds, kCue2TimeInSeconds});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample0Start, kSample0End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample1Start, kSample1End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample2Start, kSample2End)));
    // Cue before the sample end is processed.
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsCueEvent(_, kCue1TimeInSeconds)));
    // Cue after the samples is ignored.
    EXPECT_CALL(*Output(kTextStream), OnFlush(_));
  }

  ASSERT_OK(DispatchTextInfo(kTextStream));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample0Start, kSample0End));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample1Start, kSample1End));
  ASSERT_OK(DispatchTextSample(kTextStream, kSample2Start, kSample2End));

  ASSERT_OK(FlushAll({kTextStream}));
}

TEST_F(CueAlignmentHandlerTest, TextAudioVideoInputWithCues) {
  const size_t kTextStream = 0;
  const size_t kAudioStream = 1;
  const size_t kVideoStream = 2;

  const int64_t kSampleDuration = 1000;

  const int64_t kSample0Start = 0;
  const int64_t kSample0End = kSample0Start + kSampleDuration;
  const int64_t kSample1Start = kSample0End;
  const int64_t kSample1End = kSample1Start + kSampleDuration;
  const int64_t kSample2Start = kSample1End;
  const int64_t kSample2End = kSample2Start + kSampleDuration;

  const double kSample2StartInSeconds =
      static_cast<double>(kSample2Start) / kMsTimeScale;

  // Put the cue between two key frames.
  auto sync_points =
      CreateSyncPoints({static_cast<double>(kSample1Start) / kMsTimeScale});
  auto handler = std::make_shared<CueAlignmentHandler>(sync_points.get());
  ASSERT_OK(SetUpAndInitializeGraph(handler, kThreeInputs, kThreeOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample0Start, kSample0End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample1Start, kSample1End)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsCueEvent(_, kSample2StartInSeconds)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, _, kSample2Start, kSample2End)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(_));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample0Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample1Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsCueEvent(_, kSample2StartInSeconds)));
    EXPECT_CALL(
        *Output(kAudioStream),
        OnProcess(IsMediaSample(_, kSample2Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kAudioStream), OnFlush(_));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(_, kMsTimeScale, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample0Start, kSampleDuration, _, _)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample1Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsCueEvent(_, kSample2StartInSeconds)));
    EXPECT_CALL(
        *Output(kVideoStream),
        OnProcess(IsMediaSample(_, kSample2Start, kSampleDuration, _, _)));
    EXPECT_CALL(*Output(kVideoStream), OnFlush(_));
  }

  // Dispatch Stream Info
  ASSERT_OK(DispatchTextInfo(kTextStream));
  ASSERT_OK(DispatchAudioInfo(kAudioStream));
  ASSERT_OK(DispatchVideoInfo(kVideoStream));

  // Dispatch Sample 0
  ASSERT_OK(DispatchTextSample(kTextStream, kSample0Start, kSample0End));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample0Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample0Start, kSampleDuration,
                                kKeyFrame));

  // Dispatch Sample 1
  ASSERT_OK(DispatchTextSample(kTextStream, kSample1Start, kSample1End));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample1Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample1Start, kSampleDuration,
                                !kKeyFrame));

  // Dispatch Sample 2
  ASSERT_OK(DispatchTextSample(kTextStream, kSample2Start, kSample2End));
  ASSERT_OK(DispatchMediaSample(kAudioStream, kSample2Start, kSampleDuration,
                                kKeyFrame));
  ASSERT_OK(DispatchMediaSample(kVideoStream, kSample2Start, kSampleDuration,
                                kKeyFrame));

  ASSERT_OK(FlushAll({kTextStream, kAudioStream, kVideoStream}));
}

// TODO(kqyang): Add more tests, in particular, multi-thread tests.

}  // namespace media
}  // namespace shaka
