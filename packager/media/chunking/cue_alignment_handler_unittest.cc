// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/chunking/chunking_handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/chunking/cue_alignment_handler.h"
#include "packager/media/public/ad_cue_generator_params.h"
#include "packager/status_macros.h"
#include "packager/status_test_util.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace shaka {
namespace media {
namespace {
const size_t kOneInput = 1;
const size_t kOneOutput = 1;

const size_t kThreeInput = 3;
const size_t kThreeOutput = 3;

const bool kEncrypted = true;
const bool kKeyFrame = true;

const uint32_t kMsTimeScale = 1000;

const char* kNoId = "";
const char* kNoSettings = "";
const char* kNoPayload = "";

const size_t kChild = 0;
const size_t kParent = 0;
}  // namespace

class CueAlignmentHandlerTest : public MediaHandlerTestBase {
 protected:
  Status DispatchStreamInfo(size_t stream,
                            std::shared_ptr<const StreamInfo> info) {
    return Input(stream)->Dispatch(
        StreamData::FromStreamInfo(kChild, std::move(info)));
  }

  Status DispatchMediaSample(size_t stream,
                             std::shared_ptr<const MediaSample> sample) {
    return Input(stream)->Dispatch(
        StreamData::FromMediaSample(kChild, std::move(sample)));
  }

  Status DispatchTextSample(size_t stream,
                            std::shared_ptr<const TextSample> sample) {
    return Input(stream)->Dispatch(
        StreamData::FromTextSample(kChild, std::move(sample)));
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

  AdCueGeneratorParams params;
  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample0Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample1Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample2Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream), OnFlush(kParent));
  }

  auto stream_info = GetVideoStreamInfo(kMsTimeScale);
  auto sample_0 = GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame);
  auto sample_1 = GetMediaSample(kSample1Start, kSampleDuration, !kKeyFrame);
  auto sample_2 = GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame);

  DispatchStreamInfo(kVideoStream, std::move(stream_info));
  DispatchMediaSample(kVideoStream, std::move(sample_0));
  DispatchMediaSample(kVideoStream, std::move(sample_1));
  DispatchMediaSample(kVideoStream, std::move(sample_2));

  ASSERT_OK(FlushAll({kVideoStream}));
}

TEST_F(CueAlignmentHandlerTest, AudioInputWithNoCues) {
  const size_t kAudioStream = 0;

  const int64_t kSampleDuration = 1000;
  const int64_t kSample0Start = 0;
  const int64_t kSample1Start = kSample0Start + kSampleDuration;
  const int64_t kSample2Start = kSample1Start + kSampleDuration;

  AdCueGeneratorParams params;
  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample0Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample1Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample2Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream), OnFlush(kParent));
  }

  auto stream_info = GetAudioStreamInfo(kMsTimeScale);
  auto sample_0 = GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame);
  auto sample_1 = GetMediaSample(kSample1Start, kSampleDuration, kKeyFrame);
  auto sample_2 = GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame);

  ASSERT_OK(DispatchStreamInfo(kAudioStream, std::move(stream_info)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(sample_0)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(sample_1)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(sample_2)));

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

  AdCueGeneratorParams params;
  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  auto stream_info = GetTextStreamInfo(kMsTimeScale);
  auto sample_0 = GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload);
  auto sample_1 = GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload);
  auto sample_2 = GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload);

  ASSERT_OK(DispatchStreamInfo(kTextStream, std::move(stream_info)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_0)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_1)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_2)));

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

  AdCueGeneratorParams params;
  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kThreeInput, kThreeOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample0Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample1Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample2Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream), OnFlush(kParent));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample0Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample1Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample2Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream), OnFlush(kParent));
  }

  // Text samples
  auto text_stream_info = GetTextStreamInfo(kMsTimeScale);
  auto text_sample_0 =
      GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload);
  auto text_sample_1 =
      GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload);
  auto text_sample_2 =
      GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload);

  // Audio samples
  auto audio_stream_info = GetAudioStreamInfo(kMsTimeScale);
  auto audio_sample_0 =
      GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame);
  auto audio_sample_1 =
      GetMediaSample(kSample1Start, kSampleDuration, kKeyFrame);
  auto audio_sample_2 =
      GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame);

  // Video samples
  auto video_stream_info = GetVideoStreamInfo(kMsTimeScale);
  auto video_sample_0 =
      GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame);
  auto video_sample_1 =
      GetMediaSample(kSample1Start, kSampleDuration, !kKeyFrame);
  auto video_sample_2 =
      GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame);

  // Dispatch Stream Info
  ASSERT_OK(DispatchStreamInfo(kTextStream, std::move(text_stream_info)));
  ASSERT_OK(DispatchStreamInfo(kAudioStream, std::move(audio_stream_info)));
  ASSERT_OK(DispatchStreamInfo(kVideoStream, std::move(video_stream_info)));

  // Dispatch Sample 0
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(text_sample_0)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(audio_sample_0)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(video_sample_0)));

  // Dispatch Sample 1
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(text_sample_1)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(audio_sample_1)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(video_sample_1)));

  // Dispatch Sample 2
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(text_sample_2)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(audio_sample_2)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(video_sample_2)));

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
  Cuepoint cue;
  cue.start_time_in_seconds = static_cast<double>(kSample1Start) / kMsTimeScale;

  AdCueGeneratorParams params;
  params.cue_points.push_back(cue);

  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample0Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample1Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsCueEvent(kParent, kSample2StartInSeconds)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample2Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream), OnFlush(kParent));
  }

  auto stream_info = GetVideoStreamInfo(kMsTimeScale);
  auto sample_0 = GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame);
  auto sample_1 = GetMediaSample(kSample1Start, kSampleDuration, !kKeyFrame);
  auto sample_2 = GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame);

  ASSERT_OK(DispatchStreamInfo(kVideoStream, std::move(stream_info)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(sample_0)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(sample_1)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(sample_2)));

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

  Cuepoint cue;
  cue.start_time_in_seconds = static_cast<double>(kSample1Start) / kMsTimeScale;

  AdCueGeneratorParams params;
  params.cue_points.push_back(cue);

  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample0Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsCueEvent(kParent, kSample1StartInSeconds)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample1Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample2Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream), OnFlush(kParent));
  }

  auto stream_info = GetAudioStreamInfo(kMsTimeScale);
  auto sample_0 = GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame);
  auto sample_1 = GetMediaSample(kSample1Start, kSampleDuration, kKeyFrame);
  auto sample_2 = GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame);

  ASSERT_OK(DispatchStreamInfo(kAudioStream, std::move(stream_info)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(sample_0)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(sample_1)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(sample_2)));

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

  Cuepoint cue;
  cue.start_time_in_seconds = static_cast<double>(kSample1Start) / kMsTimeScale;

  AdCueGeneratorParams params;
  params.cue_points.push_back(cue);

  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsCueEvent(kParent, kSample1StartInSeconds)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  auto stream_info = GetTextStreamInfo(kMsTimeScale);
  auto sample_0 = GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload);
  auto sample_1 = GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload);
  auto sample_2 = GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload);

  ASSERT_OK(DispatchStreamInfo(kTextStream, std::move(stream_info)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_0)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_1)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_2)));

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

  const double kCueTimeInSeconds =
      static_cast<double>(kSample2Start + kSample2End) / kMsTimeScale;

  // Put the cue between the start and end of the last sample.
  Cuepoint cue;
  cue.start_time_in_seconds = kCueTimeInSeconds;

  AdCueGeneratorParams params;
  params.cue_points.push_back(cue);

  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kOneInput, kOneOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsCueEvent(kParent, kCueTimeInSeconds)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  auto stream_info = GetTextStreamInfo(kMsTimeScale);
  auto sample_0 = GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload);
  auto sample_1 = GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload);
  auto sample_2 = GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload);

  ASSERT_OK(DispatchStreamInfo(kTextStream, std::move(stream_info)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_0)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_1)));
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(sample_2)));

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
  Cuepoint cue;
  cue.start_time_in_seconds = static_cast<double>(kSample1Start) / kMsTimeScale;

  AdCueGeneratorParams params;
  params.cue_points.push_back(cue);

  SyncPointQueue sync_points(params);
  std::shared_ptr<MediaHandler> handler =
      std::make_shared<CueAlignmentHandler>(&sync_points);
  ASSERT_OK(SetUpAndInitializeGraph(handler, kThreeInput, kThreeOutput));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsCueEvent(kParent, kSample2StartInSeconds)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(_, kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample0Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample1Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsCueEvent(kParent, kSample2StartInSeconds)));
    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsMediaSample(kParent, kSample2Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kAudioStream), OnFlush(kParent));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted, _)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample0Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample1Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsCueEvent(kParent, kSample2StartInSeconds)));
    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsMediaSample(kParent, kSample2Start, kSampleDuration,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kVideoStream), OnFlush(kParent));
  }

  // Text samples
  auto text_stream_info = GetTextStreamInfo(kMsTimeScale);
  auto text_sample_0 =
      GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload);
  auto text_sample_1 =
      GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload);
  auto text_sample_2 =
      GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload);

  // Audio samples
  auto audio_stream_info = GetAudioStreamInfo(kMsTimeScale);
  auto audio_sample_0 =
      GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame);
  auto audio_sample_1 =
      GetMediaSample(kSample1Start, kSampleDuration, kKeyFrame);
  auto audio_sample_2 =
      GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame);

  // Video samples
  auto video_stream_info = GetVideoStreamInfo(kMsTimeScale);
  auto video_sample_0 =
      GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame);
  auto video_sample_1 =
      GetMediaSample(kSample1Start, kSampleDuration, !kKeyFrame);
  auto video_sample_2 =
      GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame);

  // Dispatch Stream Info
  ASSERT_OK(DispatchStreamInfo(kTextStream, std::move(text_stream_info)));
  ASSERT_OK(DispatchStreamInfo(kAudioStream, std::move(audio_stream_info)));
  ASSERT_OK(DispatchStreamInfo(kVideoStream, std::move(video_stream_info)));

  // Dispatch Sample 0
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(text_sample_0)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(audio_sample_0)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(video_sample_0)));

  // Dispatch Sample 1
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(text_sample_1)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(audio_sample_1)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(video_sample_1)));

  // Dispatch Sample 2
  ASSERT_OK(DispatchTextSample(kTextStream, std::move(text_sample_2)));
  ASSERT_OK(DispatchMediaSample(kAudioStream, std::move(audio_sample_2)));
  ASSERT_OK(DispatchMediaSample(kVideoStream, std::move(video_sample_2)));

  ASSERT_OK(FlushAll({kTextStream, kAudioStream, kVideoStream}));
}

// TODO(kqyang): Add more tests, in particular, multi-thread tests.

}  // namespace media
}  // namespace shaka
