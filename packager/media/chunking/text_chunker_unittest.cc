// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/chunking/text_chunker.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {

namespace {
const uint64_t kStreamIndex = 0;

const size_t kInputs = 1;
const size_t kOutputs = 1;

const size_t kInput = 0;
const size_t kOutput = 0;

const bool kEncrypted = true;
const bool kSubSegment = true;

const int64_t kTick = 50;

const char* kNoId = "";
const char* kNoSettings = "";
const char* kNoPayload = "";
}  // namespace

class TextChunkerTest : public MediaHandlerTestBase {
 protected:
  void Init(int64_t segment_duration) {
    ASSERT_OK(SetUpAndInitializeGraph(
        std::make_shared<TextChunker>(segment_duration), kInputs, kOutputs));
  }
};

// S0        S1
// |         |
// |[---A---]|
// |         |
TEST_F(TextChunkerTest, SampleEndingOnSegmentStart) {
  const int64_t kSegmentStart = 0;
  const int64_t kSegmentDuration = kTick;

  const int64_t kSampleStart = 0;
  const int64_t kSampleDuration = kTick;
  const int64_t kSampleEnd = kSampleStart + kSampleDuration;

  Init(kSegmentDuration);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(kStreamIndex)));

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSampleStart, kSampleEnd,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));
    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetTextStreamInfo())));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleStart, kSampleEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// S0         S1        S2
// |          |         |
// |[-A-]     |         |
// |          |[-B-]    |
// |          |         |
TEST_F(TextChunkerTest, CreatesSegmentsForSamples) {
  const int64_t kSegmentDuration = 2 * kTick;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = kSegment0Start + kSegmentDuration;

  const int64_t kSample0Start = 0;
  const int64_t kSample0Duration = kTick;
  const int64_t kSample0End = kSample0Start + kSample0Duration;

  const int64_t kSample1Start = 3 * kTick;
  const int64_t kSample1Duration = kTick;
  const int64_t kSample1End = kSample1Start + kSample1Duration;

  Init(kSegmentDuration);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(kStreamIndex)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment Two
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetTextStreamInfo())));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// S0         S1         S2        S3
// |          |          |         |
// |[-A-]     |          |         |
// |          |          |[-B-]    |
// |          |          |         |
TEST_F(TextChunkerTest, OutputsEmptySegments) {
  const int64_t kSegmentDuration = 2 * kTick;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = kSegment0Start + kSegmentDuration;
  const int64_t kSegment2Start = kSegment1Start + kSegmentDuration;

  const int64_t kSample0Start = 0;
  const int64_t kSample0Duration = kTick;
  const int64_t kSample0End = kSample0Start + kSample0Duration;

  const int64_t kSample1Start = 4 * kTick;
  const int64_t kSample1Duration = kTick;
  const int64_t kSample1End = kSample1Start + kSample1Duration;

  Init(kSegmentDuration);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(kStreamIndex)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment Two (empty segment)
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment Three
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetTextStreamInfo())));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// S0        S1        S2
// |         |         |
// |     [---A---]     |
// |         |         |
TEST_F(TextChunkerTest, SampleCrossesSegments) {
  const int64_t kSegmentDuration = 2 * kTick;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = kSegment0Start + kSegmentDuration;

  const int64_t kSampleStart = kTick;
  const int64_t kSampleDuration = 2 * kTick;
  const int64_t kSampleEnd = kSampleStart + kSampleDuration;

  Init(kSegmentDuration);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(kStreamIndex)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSampleStart, kSampleEnd,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment Two
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSampleStart, kSampleEnd,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetTextStreamInfo())));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleStart, kSampleEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// S0         S1         S2         S3
// |          |          |          |
// |   [-A----|----]     |          |
// |   [-B----|----]     |          |
// |   [-C----|----------|----]     |
// |          |          |          |
TEST_F(TextChunkerTest, PreservesOrder) {
  const int64_t kSegmentDuration = 2 * kTick;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = kSegment0Start + kSegmentDuration;
  const int64_t kSegment2Start = kSegment1Start + kSegmentDuration;

  const int64_t kSample0Start = kTick;
  const int64_t kSample0Duration = 2 * kTick;
  const int64_t kSample0End = kSample0Start + kSample0Duration;

  const int64_t kSample1Start = kTick;
  const int64_t kSample1Duration = 2 * kTick;
  const int64_t kSample1End = kSample1Start + kSample1Duration;

  const int64_t kSample2Start = kTick;
  const int64_t kSample2Duration = 4 * kTick;
  const int64_t kSample2End = kSample2Start + kSample2Duration;

  const char* kSample0Id = "sample 0";
  const char* kSample1Id = "sample 1";
  const char* kSample2Id = "sample 2";

  Init(kSegmentDuration);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(kStreamIndex)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kSample0Id, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kSample1Id, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kSample2Id, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment Two
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kSample0Id, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kSample1Id, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kSample2Id, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment Two
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kSample2Id, kSample2Start, kSample2End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetTextStreamInfo())));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kSample0Id, kSample0Start, kSample0End, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kSample1Id, kSample1Start, kSample1End, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kSample2Id, kSample2Start, kSample2End, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// S0    S1    S2    S3    S4    S5
// |     |     |     |     |     |
// |  [--|-----|--A--|-----|--]  |
// |     |  [--|--B--|--]  |     |
// |     |     |     |     |     |
TEST_F(TextChunkerTest, NestedSamples) {
  const int64_t kSegmentDuration = 2 * kTick;

  const int64_t kSample0Start = 1 * kTick;
  const int64_t kSample0Duration = 8 * kTick;
  const int64_t kSample0End = kSample0Start + kSample0Duration;

  const int64_t kSample1Start = 3 * kTick;
  const int64_t kSample1Duration = 4 * kTick;
  const int64_t kSample1End = kSample1Start + kSample1Duration;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = kSegment0Start + kSegmentDuration;
  const int64_t kSegment2Start = kSegment1Start + kSegmentDuration;
  const int64_t kSegment3Start = kSegment2Start + kSegmentDuration;
  const int64_t kSegment4Start = kSegment3Start + kSegmentDuration;

  Init(kSegmentDuration);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(kStreamIndex)));

    // Segment 0
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment 1
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment 2
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment 3
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment3Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment 4
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment4Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetTextStreamInfo())));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// S0         S1        S2         S3
// |          |         |          |
// |   [------A--------]|          |
// |          |         |[--B--]   |
// |          |         |          |
TEST_F(TextChunkerTest, SecondSampleStartsAfterMultiSegmentSampleEnds) {
  const int64_t kSegmentDuration = 2 * kTick;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = kSegment0Start + kSegmentDuration;
  const int64_t kSegment2Start = kSegment1Start + kSegmentDuration;

  const int64_t kSample0Start = kTick;
  const int64_t kSample0Duration = 3 * kTick;
  const int64_t kSample0End = kSample0Start + kSample0Duration;

  const int64_t kSample1Start = 4 * kTick;
  const int64_t kSample1Duration = kTick;
  const int64_t kSample1End = kSample1Start + kSample1Duration;

  Init(kSegmentDuration);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(kStreamIndex)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment Two
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample0Start, kSample0End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    // Segment Three
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSample1Start, kSample1End,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start, kSegmentDuration,
                                !kSubSegment, !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetTextStreamInfo())));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSample0Start, kSample0End, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSample1Start, kSample1End, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// S0        C0         C1         S1          *          *
// s0        s1         s2         *           *          s3
// |         |          |                                 |
// |     [---|-----A----|---]                             |
// |         |          |                                 |
// The segment duration is 8 ticks, but with the cues being injected, c0 will
// become s1, c1 will become s3, and S1 will become s3.
TEST_F(TextChunkerTest, SampleSpanningMultipleCues) {
  const int64_t kSegmentDuration = 8 * kTick;

  const int64_t kSampleStart = kTick;
  const int64_t kSampleDuration = 4 * kTick;
  const int64_t kSampleEnd = kSampleStart + kSampleDuration;

  const int64_t kCue0Time = 2 * kTick;
  const double kCue0TimeInSeconds = kCue0Time / 1000.0;
  const int64_t kCue1Time = 4 * kTick;
  const double kCue1TimeInSeconds = kCue1Time / 1000.0;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment0Duration = 2 * kTick;
  const int64_t kSegment1Start = kSegment0Start + kSegment0Duration;
  const int64_t kSegment1Duration = 2 * kTick;
  const int64_t kSegment2Start = kSegment1Start + kSegment1Duration;
  const int64_t kSegment2Duration = 8 * kTick;

  Init(kSegmentDuration);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(kStreamIndex)));

    // Segment 0 and Cue 0
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSampleStart, kSampleEnd,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start, kSegment0Duration,
                                !kSubSegment, !kEncrypted)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsCueEvent(kStreamIndex, kCue0TimeInSeconds)));

    // Segment 1 and Cue 1
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSampleStart, kSampleEnd,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start, kSegment1Duration,
                                !kSubSegment, !kEncrypted)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsCueEvent(kStreamIndex, kCue1TimeInSeconds)));

    // Segment 2
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(kNoId, kSampleStart, kSampleEnd,
                                       kNoSettings, kNoPayload)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start, kSegment2Duration,
                                !kSubSegment, !kEncrypted)));
  }

  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromStreamInfo(kStreamIndex, GetTextStreamInfo())));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleStart, kSampleEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromCueEvent(kStreamIndex, GetCueEvent(kCue0TimeInSeconds))));
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromCueEvent(kStreamIndex, GetCueEvent(kCue1TimeInSeconds))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

}  // namespace media
}  // namespace shaka
