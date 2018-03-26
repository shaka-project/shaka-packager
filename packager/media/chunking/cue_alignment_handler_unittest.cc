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
#include "packager/status_test_util.h"

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

const uint64_t kNoTimeScale = 0;
const uint64_t kMsTimeScale = 1000;

const char* kNoId = "";
const char* kNoSettings = "";
const char* kNoPayload = "";

const size_t kChild = 0;
const size_t kParent = 0;
}  // namespace

class CueAlignmentHandlerTest : public MediaHandlerTestBase {};

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
  SetUpAndInitializeGraph(handler, kOneInput, kOneOutput);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted)));
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

  Input(kVideoStream)
      ->Dispatch(
          StreamData::FromStreamInfo(kChild, GetVideoStreamInfo(kMsTimeScale)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample1Start, kSampleDuration, !kKeyFrame)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame)));
  Input(kVideoStream)->FlushAllDownstreams();
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
  SetUpAndInitializeGraph(handler, kOneInput, kOneOutput);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted)));
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

  Input(kAudioStream)
      ->Dispatch(
          StreamData::FromStreamInfo(kChild, GetAudioStreamInfo(kMsTimeScale)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample1Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)->FlushAllDownstreams();
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
  SetUpAndInitializeGraph(handler, kOneInput, kOneOutput);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kNoTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  Input(kTextStream)
      ->Dispatch(StreamData::FromStreamInfo(kChild, GetTextStreamInfo()));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload)));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload)));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload)));
  Input(kTextStream)->FlushAllDownstreams();
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
  SetUpAndInitializeGraph(handler, kThreeInput, kThreeOutput);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kNoTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted)));
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
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted)));
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

  Input(kTextStream)
      ->Dispatch(StreamData::FromStreamInfo(kChild, GetTextStreamInfo()));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload)));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload)));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload)));
  Input(kTextStream)->FlushAllDownstreams();

  Input(kAudioStream)
      ->Dispatch(
          StreamData::FromStreamInfo(kChild, GetAudioStreamInfo(kMsTimeScale)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample1Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)->FlushAllDownstreams();

  Input(kVideoStream)
      ->Dispatch(
          StreamData::FromStreamInfo(kChild, GetVideoStreamInfo(kMsTimeScale)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample1Start, kSampleDuration, !kKeyFrame)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame)));
  Input(kVideoStream)->FlushAllDownstreams();
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
  SetUpAndInitializeGraph(handler, kOneInput, kOneOutput);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kVideoStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted)));
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

  Input(kVideoStream)
      ->Dispatch(
          StreamData::FromStreamInfo(kChild, GetVideoStreamInfo(kMsTimeScale)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample1Start, kSampleDuration, !kKeyFrame)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame)));
  Input(kVideoStream)->FlushAllDownstreams();
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
  SetUpAndInitializeGraph(handler, kOneInput, kOneOutput);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted)));
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

  Input(kAudioStream)
      ->Dispatch(
          StreamData::FromStreamInfo(kChild, GetAudioStreamInfo(kMsTimeScale)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample1Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)->FlushAllDownstreams();
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
  SetUpAndInitializeGraph(handler, kOneInput, kOneOutput);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kNoTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsCueEvent(kParent, kSample1StartInSeconds)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  Input(kTextStream)
      ->Dispatch(StreamData::FromStreamInfo(kChild, GetTextStreamInfo()));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload)));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload)));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload)));
  Input(kTextStream)->FlushAllDownstreams();
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
  SetUpAndInitializeGraph(handler, kThreeInput, kThreeOutput);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsStreamInfo(kParent, kNoTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsCueEvent(kParent, kSample2StartInSeconds)));
    EXPECT_CALL(*Output(kTextStream),
                OnProcess(IsTextSample(kNoId, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kTextStream), OnFlush(kParent));
  }

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kAudioStream),
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted)));
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
                OnProcess(IsStreamInfo(kParent, kMsTimeScale, !kEncrypted)));
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

  Input(kTextStream)
      ->Dispatch(StreamData::FromStreamInfo(kChild, GetTextStreamInfo()));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload)));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload)));
  Input(kTextStream)
      ->Dispatch(StreamData::FromTextSample(
          kChild,
          GetTextSample(kNoId, kSample2Start, kSample2End, kNoPayload)));
  Input(kTextStream)->FlushAllDownstreams();

  Input(kAudioStream)
      ->Dispatch(
          StreamData::FromStreamInfo(kChild, GetAudioStreamInfo(kMsTimeScale)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample1Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame)));
  Input(kAudioStream)->FlushAllDownstreams();

  Input(kVideoStream)
      ->Dispatch(
          StreamData::FromStreamInfo(kChild, GetVideoStreamInfo(kMsTimeScale)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample0Start, kSampleDuration, kKeyFrame)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample1Start, kSampleDuration, !kKeyFrame)));
  Input(kVideoStream)
      ->Dispatch(StreamData::FromMediaSample(
          kChild, GetMediaSample(kSample2Start, kSampleDuration, kKeyFrame)));
  Input(kVideoStream)->FlushAllDownstreams();
}

// TODO(kqyang): Add more tests, in particular, multi-thread tests.

}  // namespace media
}  // namespace shaka
