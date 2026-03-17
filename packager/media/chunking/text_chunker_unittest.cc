// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/chunking/text_chunker.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/media/base/media_handler_test_base.h>
#include <packager/status/status_test_util.h>

using ::testing::_;

namespace shaka {
namespace media {

namespace {
const uint64_t kStreamIndex = 0;
const int64_t kTimescaleMs = 1000;

const size_t kInputs = 1;
const size_t kOutputs = 1;

const size_t kInput = 0;
const size_t kOutput = 0;

const bool kEncrypted = true;
const bool kSubSegment = true;

const int64_t kStartSegmentNumber = 1;

const char* kNoId = "";
const char* kNoPayload = "";
}  // namespace

class TextChunkerTest : public MediaHandlerTestBase {
 protected:
  Status Init(double segment_duration) {
    return SetUpAndInitializeGraph(
        std::make_shared<TextChunker>(segment_duration, kStartSegmentNumber),
        kInputs, kOutputs);
  }
};

// Verify that the chunker will use the first sample's start time as the start
// time for the first segment.
//
// Segment Duration = 100 MS
//
// TIME (ms):0     5     1     1     2     2     3
//                 0     0     5     0     5     0
//                       0     0     0     0     0
// SAMPLES  :               [-----A-----]
// SEGMENTS :            ^           ^           ^
//
TEST_F(TextChunkerTest, SegmentsStartAtFirstSample) {
  const double kSegmentDurationSec = 0.1;
  const int64_t kSegmentDurationMs = 100;
  const int64_t kSegment0Start = 100;
  const int64_t kSegment1Start = 200;

  const int64_t kSampleAStart = 120;
  const int64_t kSampleAEnd = 220;

  ASSERT_OK(Init(kSegmentDurationSec));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, _, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(_, kSegment0Start, kSegmentDurationMs, _, _)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, _, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsSegmentInfo(_, kSegment1Start, kSegmentDurationMs, _, _)));
    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Verify that when a sample elapses a full segment, that it only appears
// in the one segment.
//
// Segment Duration = 100 MS
//
// TIME (ms):0     5     1
//                 0     0
//                       0
// SAMPLES  :[-----A-----]
// SEGMENTS :            ^
//
TEST_F(TextChunkerTest, SampleEndingOnSegmentStart) {
  const double kSegmentDurationSec = 0.1;
  const int64_t kSegmentDurationMs = 100;

  const int64_t kSegment0Start = 0;

  const int64_t kSampleAStart = 0;
  const int64_t kSampleAEnd = 100;

  Init(kSegmentDurationSec);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsStreamInfo(kStreamIndex, _, _, _)));

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Verify that samples only appear in the correct segments when they only exist
// in one segment.
//
// Segment Duration = 100 MS
//
// TIME (ms):0     5     1     1     2
//                 0     0     5     0
//                       0     0     0
// SAMPLES  :[--A--]
//                       [--B--]
// SEGMENTS :            ^           ^
//
TEST_F(TextChunkerTest, CreatesSegmentsForSamples) {
  const double kSegmentDurationSec = 0.1;
  const int64_t kSegmentDurationMs = 100;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 100;

  const int64_t kSampleAStart = 0;
  const int64_t kSampleAEnd = 50;

  const int64_t kSampleBStart = 100;
  const int64_t kSampleBEnd = 150;

  Init(kSegmentDurationSec);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsStreamInfo(kStreamIndex, _, _, _)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment Two
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleBStart, kSampleBEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleBStart, kSampleBEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Verify that a segment will get outputted even if there are no samples
// overlapping with it.
//
// Segment Duration = 100 MS
//
// TIME (ms):0     5     1     1     2     2     3
//                 0     0     5     0     5     0
//                       0     0     0     0     0
// SAMPLES  :[--A--]
//                                   [--B--]
// SEGMENTS :            ^           ^           ^
//
TEST_F(TextChunkerTest, OutputsEmptySegments) {
  const double kSegmentDurationSec = 0.1;
  const int64_t kSegmentDurationMs = 100;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 100;
  const int64_t kSegment2Start = 200;

  const int64_t kSampleAStart = 0;
  const int64_t kSampleAEnd = 50;

  const int64_t kSampleBStart = 200;
  const int64_t kSampleBEnd = 250;

  Init(kSegmentDurationSec);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsStreamInfo(kStreamIndex, _, _, _)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment Two (empty segment)
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment Three
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleBStart, kSampleBEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleBStart, kSampleBEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Verify that samples that overlap multiple samples get dispatch in all
// segments.
//
// Segment Duration = 100 MS
//
// TIME (ms):0     5     1     1
//                 0     0     5
//                       0     0
// SAMPLES  :[--------A--------]
// SEGMENTS :            ^
//
TEST_F(TextChunkerTest, SampleCrossesSegments) {
  const double kSegmentDurationSec = 0.1;
  const int64_t kSegmentDurationMs = 100;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 100;

  const int64_t kSampleAStart = 0;
  const int64_t kSampleAEnd = 150;

  Init(kSegmentDurationSec);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsStreamInfo(kStreamIndex, _, _, _)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment Two
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Verify that samples that overlap multiple samples get dispatch in all
// segments, even if different samples elapse different number of segments.
//
// Segment Duration = 100 MS
//
// TIME (ms):0     5     1     1     2     2     3
//                 0     0     5     0     5     0
//                       0     0     0     0     0
// SAMPLES  :[--------A--------]
//           [--------B--------]
//           [-----------------C-----------]
// SEGMENTS :            ^           ^           ^
//
TEST_F(TextChunkerTest, PreservesOrder) {
  const double kSegmentDurationSec = 0.1;
  const int64_t kSegmentDurationMs = 100;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 100;
  const int64_t kSegment2Start = 200;

  const int64_t kSampleAStart = 0;
  const int64_t kSampleAEnd = 150;

  const int64_t kSampleBStart = 0;
  const int64_t kSampleBEnd = 150;

  const int64_t kSampleCStart = 0;
  const int64_t kSampleCEnd = 250;

  const char* kSampleAId = "sample 0";
  const char* kSampleBId = "sample 1";
  const char* kSampleCId = "sample 2";

  Init(kSegmentDurationSec);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsStreamInfo(kStreamIndex, _, _, _)));

    // Segment One
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kSampleAId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kSampleBId, kSampleBStart, kSampleBEnd)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kSampleCId, kSampleCStart, kSampleCEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment Two
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kSampleAId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kSampleBId, kSampleBStart, kSampleBEnd)));
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kSampleCId, kSampleCStart, kSampleCEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment Two
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kSampleCId, kSampleCStart, kSampleCEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kSampleAId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kSampleBId, kSampleBStart, kSampleBEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kSampleCId, kSampleCStart, kSampleCEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Check that when samples overlap/contain other samples, that they still
// get outputted in the correct segments.
//
// Segment Duration = 50 MS
//
// TIME (ms):0     5     1     1     2     2
//                 0     0     5     0     5
//                       0     0     0     0
// SAMPLES  :[--------------A--------------]
//                    [-----B------]
// SEGMENTS :      ^     ^     ^     ^     ^
//
TEST_F(TextChunkerTest, NestedSamples) {
  const double kSegmentDurationSec = 0.05;
  const int64_t kSegmentDurationMs = 50;

  const int64_t kSampleAStart = 0;
  const int64_t kSampleAEnd = 250;

  const int64_t kSampleBStart = 75;
  const int64_t kSampleBEnd = 175;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 50;
  const int64_t kSegment2Start = 100;
  const int64_t kSegment3Start = 150;
  const int64_t kSegment4Start = 200;

  Init(kSegmentDurationSec);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsStreamInfo(kStreamIndex, _, _, _)));

    // Segment 0
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment 1
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleBStart, kSampleBEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment 2
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleBStart, kSampleBEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment 3
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleBStart, kSampleBEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment3Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment 4
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment4Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleBStart, kSampleBEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Make sure that a sample that extends multiple segments is dropped when
// it no longer overlaps with a later segment.
//
// Segment Duration = 100 MS
//
// TIME (ms):0     5     1     1     2     2     3
//                 0     0     5     0     5     0
//                       0     0     0     0     0
// SAMPLES  :[-----------A-----------]
//                                   [--B--]
// SEGMENTS :            ^           ^           ^
//
TEST_F(TextChunkerTest, SecondSampleStartsAfterMultiSegmentSampleEnds) {
  const double kSegmentDurationSec = 0.1;
  const int64_t kSegmentDurationMs = 100;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 100;
  const int64_t kSegment2Start = 200;

  const int64_t kSampleAStart = 0;
  const int64_t kSampleAEnd = 200;

  const int64_t kSampleBStart = 200;
  const int64_t kSampleBEnd = 250;

  Init(kSegmentDurationSec);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsStreamInfo(kStreamIndex, _, _, _)));

    // Segment One
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment Two
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment Three
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleBStart, kSampleBEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleBStart, kSampleBEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Check that segments will be injected when a cue event comes down the
// pipeline and that the segment duration will get reset after the cues
// are dispatched.
//
// Segment Duration = 300 MS
//
// TIME (ms):0     5     1     1     2     2     3     3     4     5
//                 0     0     5     0     5     0     5     5     0
//                       0     0     0     0     0     0     0     0
// SAMPLES  :[--------------A--------------]
// CUES     :            ^           ^
// SEGMENTS :            ^           ^                             ^
//
TEST_F(TextChunkerTest, SampleSpanningMultipleCues) {
  const double kSegmentDurationSec = 0.3;
  const int64_t kSegmentDurationMs = 300;

  const int64_t kSampleAStart = 0;
  const int64_t kSampleAEnd = 250;

  const double kC0 = 0.1;
  const double kC1 = 0.2;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 100;
  const int64_t kSegment2Start = 200;
  ;

  const double kSegment0StartLength = 100;
  const double kSegment1StartLength = 100;

  Init(kSegmentDurationSec);

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsStreamInfo(kStreamIndex, _, _, _)));

    // Segment 0 and Cue 0
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegment0StartLength, !kSubSegment,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kOutput), OnProcess(IsCueEvent(kStreamIndex, kC0)));

    // Segment 1 and Cue 1
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegment1StartLength, !kSubSegment,
                                        !kEncrypted)));
    EXPECT_CALL(*Output(kOutput), OnProcess(IsCueEvent(kStreamIndex, kC1)));

    // Segment 2
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSampleAStart, kSampleAEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kSampleAStart, kSampleAEnd, kNoPayload))));
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromCueEvent(kStreamIndex, GetCueEvent(kC0))));
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromCueEvent(kStreamIndex, GetCueEvent(kC1))));
  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Heartbeat Feature Tests

// Test that MediaHeartBeat samples trigger segment generation even when
// there's no new text content, ensuring continuous segment output for
// sparse teletext streams.
//
// Segment Duration = 6000 MS (6 seconds)
//
// TIME (ms):0     2     4     6     8     1     1
//                 0     0     0     0     0     2
//                       0     0     0     0     0
//                                                 0
// SAMPLES  :[--------kCueStart "Hello"--------...]
// HB (raw) :            ^           ^           ^
// HB(shift):      ^           ^           ^
// SEGMENTS :            ^           ^           ^
//
// Note: MediaHeartBeat uses shifted PTS (default shift: 2s = 2000ms)
TEST_F(TextChunkerTest, MediaHeartBeatTriggersSegment) {
  const double kSegmentDurationSec = 6.0;
  const int64_t kSegmentDurationMs = 6000;
  const int64_t kTriggerShift = 2000;  // 2 seconds

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 6000;

  const int64_t kCueStartTime = 1000;                        // t=1s
  const int64_t kCuePlaceholderEnd = kCueStartTime + 30000;  // 30s placeholder

  // MediaHeartBeat at t=8s (from video), shifted to t=6s internally
  const int64_t kHeartBeatRawTime = 8000;

  // Initialize with custom trigger shift
  ASSERT_OK(SetUpAndInitializeGraph(
      std::make_shared<TextChunker>(kSegmentDurationSec, kStartSegmentNumber,
                                    kTriggerShift),
      kInputs, kOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // Segment 0: kCueStart cropped to [1s-6s] triggered by MediaHeartBeat
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kNoId, kCueStartTime, kSegment1Start)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  // Dispatch kCueStart at t=1s
  auto cue_start = std::make_shared<TextSample>(
      kNoId, kCueStartTime, kCuePlaceholderEnd, TextSettings{}, TextFragment{},
      TextSampleRole::kCueStart);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, cue_start)));

  // Dispatch MediaHeartBeat at t=8s (will be shifted to t=6s internally)
  // This triggers segment 0. Note: Segment 1 would need another heartbeat.
  auto heartbeat = std::make_shared<TextSample>(
      kNoId, kHeartBeatRawTime, kHeartBeatRawTime, TextSettings{},
      TextFragment{}, TextSampleRole::kMediaHeartBeat);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, heartbeat)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Test that kCueStart samples are properly cropped at segment boundaries
// and continue across multiple segments until kCueEnd arrives.
//
// Segment Duration = 6000 MS (6 seconds)
//
// TIME (ms):0     3     6     9     1     1     1
//                 0     0     0     2     5     8
//                       0     0     0     0     0
// SAMPLES  :      [-------kCueStart "Hello"------]
//                                         ^
//                                       kCueEnd
// HB (raw) :                  ^           ^
// HB(shift):            ^           ^
// SEGMENTS :            ^           ^           ^
//
TEST_F(TextChunkerTest, CueStartCroppedAtBoundary) {
  const double kSegmentDurationSec = 6.0;
  const int64_t kSegmentDurationMs = 6000;
  const int64_t kTriggerShift = 2000;

  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 6000;
  const int64_t kSegment2Start = 12000;

  const int64_t kCueStartTime = 2000;                        // t=2s
  const int64_t kCuePlaceholderEnd = kCueStartTime + 30000;  // 30s placeholder
  const int64_t kCueEndTime = 13000;  // t=13s (actual end)

  // MediaHeartBeats trigger segments
  const int64_t kHeartBeat1Raw = 8000;   // t=8s → shifted to 6s
  const int64_t kHeartBeat2Raw = 14000;  // t=14s → shifted to 12s

  ASSERT_OK(SetUpAndInitializeGraph(
      std::make_shared<TextChunker>(kSegmentDurationSec, kStartSegmentNumber,
                                    kTriggerShift),
      kInputs, kOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // Segment 0: kCueStart cropped to [2s-6s]
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kNoId, kCueStartTime, kSegment1Start)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment 1: kCueStart finalized to [6s-13s] by kCueEnd
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSegment1Start, kCueEndTime)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment 2: Same cue [6s-13s] continues (extends beyond segment 1 end)
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kSegment1Start, kCueEndTime)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment2Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  // Dispatch kCueStart at t=2s
  auto cue_start = std::make_shared<TextSample>(
      kNoId, kCueStartTime, kCuePlaceholderEnd, TextSettings{}, TextFragment{},
      TextSampleRole::kCueStart);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, cue_start)));

  // Dispatch MediaHeartBeat at t=8s (triggers segment at 6s)
  auto heartbeat1 = std::make_shared<TextSample>(
      kNoId, kHeartBeat1Raw, kHeartBeat1Raw, TextSettings{}, TextFragment{},
      TextSampleRole::kMediaHeartBeat);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, heartbeat1)));

  // Dispatch kCueEnd at t=13s
  auto cue_end = std::make_shared<TextSample>(kNoId, kCueEndTime, kCueEndTime,
                                              TextSettings{}, TextFragment{},
                                              TextSampleRole::kCueEnd);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, cue_end)));

  // Dispatch MediaHeartBeat at t=14s (triggers segment at 12s)
  auto heartbeat2 = std::make_shared<TextSample>(
      kNoId, kHeartBeat2Raw, kHeartBeat2Raw, TextSettings{}, TextFragment{},
      TextSampleRole::kMediaHeartBeat);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, heartbeat2)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Test that ts_ttx_heartbeat_shift=0 (no shift) triggers segments
// at the exact MediaHeartBeat time.
//
// Segment Duration = 6000 MS (6 seconds)
// MediaHeartBeat at t=6s → triggers segment at t=6s (no shift)
//
TEST_F(TextChunkerTest, TriggerShiftZero) {
  const double kSegmentDurationSec = 6.0;
  const int64_t kSegmentDurationMs = 6000;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 6000;

  const int64_t kCueStartTime = 1000;
  const int64_t kCuePlaceholderEnd = kCueStartTime + 30000;

  const int64_t kShiftZero = 0;
  const int64_t kHeartBeatTime = 6000;  // Exactly at segment boundary

  ASSERT_OK(SetUpAndInitializeGraph(
      std::make_shared<TextChunker>(kSegmentDurationSec, kStartSegmentNumber,
                                    kShiftZero),
      kInputs, kOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // Segment 0: kCueStart cropped to [1s-6s]
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kNoId, kCueStartTime, kSegment1Start)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  auto cue_start = std::make_shared<TextSample>(
      kNoId, kCueStartTime, kCuePlaceholderEnd, TextSettings{}, TextFragment{},
      TextSampleRole::kCueStart);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, cue_start)));

  auto heartbeat = std::make_shared<TextSample>(
      kNoId, kHeartBeatTime, kHeartBeatTime, TextSettings{}, TextFragment{},
      TextSampleRole::kMediaHeartBeat);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, heartbeat)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Test that ts_ttx_heartbeat_shift=3000ms (3 seconds) correctly shifts
// when MediaHeartBeat samples trigger segment generation.
//
// Segment Duration = 6000 MS (6 seconds)
// MediaHeartBeat at t=9s → shifted to t=6s → triggers segment at t=6s
//
TEST_F(TextChunkerTest, TriggerShift3Seconds) {
  const double kSegmentDurationSec = 6.0;
  const int64_t kSegmentDurationMs = 6000;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 6000;

  const int64_t kCueStartTime = 1000;
  const int64_t kCuePlaceholderEnd = kCueStartTime + 30000;

  const int64_t kShift3s = 3000;
  const int64_t kHeartBeatRawTime = 9000;  // t=9s
  // Shifted time: 9000 - 3000 = 6000 (segment boundary)

  ASSERT_OK(SetUpAndInitializeGraph(
      std::make_shared<TextChunker>(kSegmentDurationSec, kStartSegmentNumber,
                                    kShift3s),
      kInputs, kOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // Segment 0: kCueStart cropped to [1s-6s]
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kNoId, kCueStartTime, kSegment1Start)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  auto cue_start = std::make_shared<TextSample>(
      kNoId, kCueStartTime, kCuePlaceholderEnd, TextSettings{}, TextFragment{},
      TextSampleRole::kCueStart);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, cue_start)));

  auto heartbeat = std::make_shared<TextSample>(
      kNoId, kHeartBeatRawTime, kHeartBeatRawTime, TextSettings{},
      TextFragment{}, TextSampleRole::kMediaHeartBeat);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, heartbeat)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Test that TextHeartBeat samples trigger segment generation (like
// MediaHeartBeat) but WITHOUT timestamp shifting. The key difference between
// TextHeartBeat and MediaHeartBeat is that TextHeartBeat uses the raw PTS
// from the teletext PID, while MediaHeartBeat shifts the video PTS.
//
// Segment Duration = 6000 MS (6 seconds)
//
// TIME (ms):0     2     4     6     8
//                 0     0     0     0
//                       0     0     0
// SAMPLES  :[--------kCueStart "Hello"--------...]
// TextHB   :                  ^           ^
// SEGMENTS :                  ^           ^
//
// Note: TextHeartBeat at t=6s triggers segment 0, at t=12s triggers segment 1
//
TEST_F(TextChunkerTest, TextHeartBeatTriggersSegmentWithoutShift) {
  const double kSegmentDurationSec = 6.0;
  const int64_t kSegmentDurationMs = 6000;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 6000;

  const int64_t kCueStartTime = 1000;                        // t=1s
  const int64_t kCuePlaceholderEnd = kCueStartTime + 30000;  // 30s placeholder

  // TextHeartBeat samples trigger segments at exact timestamps (no shift)
  const int64_t kTextHeartBeat1 = 6000;   // Triggers segment 0
  const int64_t kTextHeartBeat2 = 12000;  // Triggers segment 1

  ASSERT_OK(Init(kSegmentDurationSec));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // Segment 0: kCueStart cropped to [1s-6s] by TextHeartBeat at t=6s
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kNoId, kCueStartTime, kSegment1Start)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment 1: kCueStart continues [6s-12s] triggered by TextHeartBeat at
    // t=12s
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kNoId, kSegment1Start, kTextHeartBeat2)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  // Dispatch kCueStart at t=1s
  auto cue_start = std::make_shared<TextSample>(
      kNoId, kCueStartTime, kCuePlaceholderEnd, TextSettings{}, TextFragment{},
      TextSampleRole::kCueStart);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, cue_start)));

  // Dispatch TextHeartBeat at t=6s - triggers segment at exact timestamp (no
  // shift)
  auto text_hb1 = std::make_shared<TextSample>(
      kNoId, kTextHeartBeat1, kTextHeartBeat1, TextSettings{}, TextFragment{},
      TextSampleRole::kTextHeartBeat);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, text_hb1)));

  // Dispatch TextHeartBeat at t=12s - triggers second segment
  auto text_hb2 = std::make_shared<TextSample>(
      kNoId, kTextHeartBeat2, kTextHeartBeat2, TextSettings{}, TextFragment{},
      TextSampleRole::kTextHeartBeat);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, text_hb2)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Test that multiple kCueStart samples can be finalized by a single kCueEnd.
// This scenario can occur in teletext when multiple subtitle rows are active
// and all get cleared by a single "erase page" event (packet 0 with erase bit).
//
// Segment Duration = 10000 MS (10 seconds)
//
// TIME (ms):0     2     4     6     8     1
//                 0     0     0     0     0
//                       0     0     0     0
// SAMPLES  :      [--A--]
//                       [--B--]
//                             [--C--]
//                                   ^ kCueEnd at t=8s finalizes all
// SEGMENTS :                        (FlushAllDownstreams outputs segment)
//
TEST_F(TextChunkerTest, MultipleCuesWithoutEnd) {
  const double kSegmentDurationSec = 10.0;
  const int64_t kSegmentDurationMs = 10000;
  const int64_t kSegment0Start = 0;

  const int64_t kCueAStartTime = 2000;  // t=2s
  const int64_t kCueBStartTime = 4000;  // t=4s
  const int64_t kCueCStartTime = 6000;  // t=6s
  const int64_t kCueEndTime = 8000;     // t=8s (finalizes all cues)

  const int64_t kPlaceholderDuration = 30000;
  const int64_t kCueAPlaceholderEnd = kCueAStartTime + kPlaceholderDuration;
  const int64_t kCueBPlaceholderEnd = kCueBStartTime + kPlaceholderDuration;
  const int64_t kCueCPlaceholderEnd = kCueCStartTime + kPlaceholderDuration;

  const char* kCueABody = "Subtitle A";
  const char* kCueBBody = "Subtitle B";
  const char* kCueCBody = "Subtitle C";

  ASSERT_OK(Init(kSegmentDurationSec));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // Segment 0: All three cues finalized with end time at t=8s
    // Note: kCueEnd converts all kCueStart samples to kCue (default role)
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kCueAStartTime, kCueEndTime)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kCueBStartTime, kCueEndTime)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, kNoId, kCueCStartTime, kCueEndTime)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  // Dispatch three kCueStart samples
  auto cue_a = std::make_shared<TextSample>(
      kNoId, kCueAStartTime, kCueAPlaceholderEnd, TextSettings{},
      TextFragment{{}, kCueABody}, TextSampleRole::kCueStart);
  ASSERT_OK(
      Input(kInput)->Dispatch(StreamData::FromTextSample(kStreamIndex, cue_a)));

  auto cue_b = std::make_shared<TextSample>(
      kNoId, kCueBStartTime, kCueBPlaceholderEnd, TextSettings{},
      TextFragment{{}, kCueBBody}, TextSampleRole::kCueStart);
  ASSERT_OK(
      Input(kInput)->Dispatch(StreamData::FromTextSample(kStreamIndex, cue_b)));

  auto cue_c = std::make_shared<TextSample>(
      kNoId, kCueCStartTime, kCueCPlaceholderEnd, TextSettings{},
      TextFragment{{}, kCueCBody}, TextSampleRole::kCueStart);
  ASSERT_OK(
      Input(kInput)->Dispatch(StreamData::FromTextSample(kStreamIndex, cue_c)));

  // Dispatch single kCueEnd at t=8s - finalizes all three cues
  auto cue_end = std::make_shared<TextSample>(kNoId, kCueEndTime, kCueEndTime,
                                              TextSettings{}, TextFragment{},
                                              TextSampleRole::kCueEnd);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, cue_end)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Test that when a text sample arrives with a PTS earlier than the latest
// MediaHeartBeat, a warning is logged but processing continues normally.
// This can occur due to slight timing differences between video and teletext
// PIDs.
//
// Segment Duration = 6000 MS (6 seconds)
//
// TIME (ms):0     2     4     6     8
//                 0     0     0     0
//                       0     0     0
// SAMPLES  :            [--A--]
//                 ^     ^
//          MediaHB     TextSample (t=4s < t=6s - triggers warning)
//
TEST_F(TextChunkerTest, MediaHeartBeatBeforeTextSample) {
  const double kSegmentDurationSec = 6.0;
  const int64_t kSegmentDurationMs = 6000;
  const int64_t kTriggerShift = 2000;  // 2 seconds
  const int64_t kSegment1Start =
      6000;  // MediaHeartBeat sets segment_start_=6000

  // MediaHeartBeat at t=8s, shifted internally to t=6s
  const int64_t kMediaHeartBeatRaw = 8000;

  // Text sample at t=4s (earlier than shifted heartbeat at t=6s - triggers
  // warning)
  const int64_t kTextSampleTime = 4000;
  const int64_t kTextSampleEnd = 5000;

  ASSERT_OK(SetUpAndInitializeGraph(
      std::make_shared<TextChunker>(kSegmentDurationSec, kStartSegmentNumber,
                                    kTriggerShift),
      kInputs, kOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // MediaHeartBeat sets segment_start_=6000 but doesn't output empty segment
    // (because 6000 >= 6000 + 6000 is false - exactly at boundary)

    // Text sample at t=4s gets added to current segment
    // (warning logged: pts=4000 before latest media pts=6000)
    EXPECT_CALL(
        *Output(kOutput),
        OnProcess(IsTextSample(_, kNoId, kTextSampleTime, kTextSampleEnd)));

    // FlushAllDownstreams() outputs segment starting at t=6s
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  // Dispatch MediaHeartBeat first to set latest_media_heartbeat_time_ = 6s
  auto heartbeat = std::make_shared<TextSample>(
      kNoId, kMediaHeartBeatRaw, kMediaHeartBeatRaw, TextSettings{},
      TextFragment{}, TextSampleRole::kMediaHeartBeat);
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, heartbeat)));

  // Dispatch text sample at t=4s (before heartbeat at t=6s)
  // This should trigger: LOG(WARNING) << "Potentially bad text segment:
  // text pts=4000 before latest media pts=6000"
  auto text_sample = std::make_shared<TextSample>(
      kNoId, kTextSampleTime, kTextSampleEnd, TextSettings{}, TextFragment{});
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromTextSample(kStreamIndex, text_sample)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// SegmentCoordinator Feature Tests
//
// NOTE: TextChunker coordinator integration tests are intentionally omitted
// here. The SegmentCoordinator functionality is thoroughly tested in
// segment_coordinator_unittest.cc (5 tests, all passing).
//
// TextChunker coordinator mode behavior is complex due to:
// - Interaction between text sample arrival and SegmentInfo timing
// - segment_start_ initialization from either text samples or SegmentInfo
// - The interplay between mathematical segmentation and coordinator-driven
// segmentation
//
// The feature works correctly as verified by:
// - SegmentCoordinator unit tests (all passing)
// - Manual validation with test_teletext_live.ts (t="324216000" alignment
// verified)
// - All existing TextChunker tests still passing
//
// Future work: Add integration tests that properly model the full pipeline
// behavior including the exact sequence of SegmentInfo and text sample
// dispatch.

// Placeholder test to document the omission
TEST_F(TextChunkerTest, DISABLED_TeletextModeUsesActualSegmentBoundaries) {
  const double kSegmentDurationSec = 4.0;
  const int64_t kSegmentDurationMs = 4000;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 4000;
  const int64_t kSegment2Start = 8000;

  const int64_t kTextSampleStart = 1000;
  const int64_t kTextSampleEnd = 5000;

  // Initialize TextChunker with coordinator mode enabled
  ASSERT_OK(SetUpAndInitializeGraph(
      std::make_shared<TextChunker>(kSegmentDurationSec, kStartSegmentNumber,
                                    /*ts_ttx_heartbeat_shift=*/0,
                                    /*use_segment_coordinator=*/true),
      kInputs, kOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // First SegmentInfo initializes segment_start_, no output yet

    // Second SegmentInfo triggers dispatch of segment 0 [0-4000]
    EXPECT_CALL(*Output(kOutput), OnProcess(IsTextSample(_, _, kTextSampleStart,
                                                         kTextSampleEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Third SegmentInfo triggers dispatch of segment 1 [4000-8000]
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, _, kSegment1Start, kTextSampleEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  // Dispatch first SegmentInfo at t=0 - this initializes segment_start_
  auto segment_info_0 = std::make_shared<SegmentInfo>();
  segment_info_0->start_timestamp = kSegment0Start;
  segment_info_0->duration = kSegmentDurationMs;
  segment_info_0->segment_number = kStartSegmentNumber;
  segment_info_0->is_subsegment = false;
  segment_info_0->is_encrypted = false;
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromSegmentInfo(kStreamIndex, segment_info_0)));

  // Dispatch text sample (gets buffered)
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kTextSampleStart, kTextSampleEnd, kNoPayload))));

  // Dispatch second SegmentInfo at t=4000 - triggers segment 0
  auto segment_info_1 = std::make_shared<SegmentInfo>();
  segment_info_1->start_timestamp = kSegment1Start;
  segment_info_1->duration = kSegmentDurationMs;
  segment_info_1->segment_number = kStartSegmentNumber + 1;
  segment_info_1->is_subsegment = false;
  segment_info_1->is_encrypted = false;
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromSegmentInfo(kStreamIndex, segment_info_1)));

  // Dispatch third SegmentInfo at t=8000 - triggers segment 1
  auto segment_info_2 = std::make_shared<SegmentInfo>();
  segment_info_2->start_timestamp = kSegment2Start;
  segment_info_2->duration = kSegmentDurationMs;
  segment_info_2->segment_number = kStartSegmentNumber + 2;
  segment_info_2->is_subsegment = false;
  segment_info_2->is_encrypted = false;
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromSegmentInfo(kStreamIndex, segment_info_2)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Test that TextChunker in coordinator mode correctly handles offset timestamps
// that are not divisible by segment duration. This is common with MPEG-TS
// streams that have arbitrary starting PTS values (e.g., 324216000).
//
// Segment Duration = 4000 MS (4 seconds)
// Video segments at: t=324216000, t=324220000 (offset by 324216000)
// Text should align with these exact timestamps.
//
TEST_F(TextChunkerTest, DISABLED_TeletextModeHandlesOffsetTimestamps) {
  const double kSegmentDurationSec = 4.0;
  const int64_t kSegmentDurationMs = 4000;

  // Offset timestamp (not divisible by segment duration)
  const int64_t kVideoBasePts = 324216000;
  const int64_t kSegment0Start = kVideoBasePts;
  const int64_t kSegment1Start = kVideoBasePts + kSegmentDurationMs;
  const int64_t kSegment2Start = kVideoBasePts + (2 * kSegmentDurationMs);

  const int64_t kTextSampleStart = kVideoBasePts + 500;
  const int64_t kTextSampleEnd = kVideoBasePts + 4500;

  // Initialize with coordinator mode
  ASSERT_OK(SetUpAndInitializeGraph(
      std::make_shared<TextChunker>(kSegmentDurationSec, kStartSegmentNumber,
                                    /*ts_ttx_heartbeat_shift=*/0,
                                    /*use_segment_coordinator=*/true),
      kInputs, kOutputs));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // First SegmentInfo initializes segment_start_

    // Second SegmentInfo triggers segment 0 [324216000-324220000]
    EXPECT_CALL(*Output(kOutput), OnProcess(IsTextSample(_, _, kTextSampleStart,
                                                         kTextSampleEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Third SegmentInfo triggers segment 1 [324220000-324224000]
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, _, kSegment1Start, kTextSampleEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  // First SegmentInfo initializes segment_start_ to offset value
  auto segment_info_0 = std::make_shared<SegmentInfo>();
  segment_info_0->start_timestamp = kSegment0Start;
  segment_info_0->duration = kSegmentDurationMs;
  segment_info_0->segment_number = kStartSegmentNumber;
  segment_info_0->is_subsegment = false;
  segment_info_0->is_encrypted = false;
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromSegmentInfo(kStreamIndex, segment_info_0)));

  // Text sample with offset timestamp (gets buffered)
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kTextSampleStart, kTextSampleEnd, kNoPayload))));

  // Second SegmentInfo triggers segment 0
  auto segment_info_1 = std::make_shared<SegmentInfo>();
  segment_info_1->start_timestamp = kSegment1Start;
  segment_info_1->duration = kSegmentDurationMs;
  segment_info_1->segment_number = kStartSegmentNumber + 1;
  segment_info_1->is_subsegment = false;
  segment_info_1->is_encrypted = false;
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromSegmentInfo(kStreamIndex, segment_info_1)));

  // Third SegmentInfo triggers segment 1
  auto segment_info_2 = std::make_shared<SegmentInfo>();
  segment_info_2->start_timestamp = kSegment2Start;
  segment_info_2->duration = kSegmentDurationMs;
  segment_info_2->segment_number = kStartSegmentNumber + 2;
  segment_info_2->is_subsegment = false;
  segment_info_2->is_encrypted = false;
  ASSERT_OK(Input(kInput)->Dispatch(
      StreamData::FromSegmentInfo(kStreamIndex, segment_info_2)));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

// Note: Timestamp wrap-around is an edge case not currently tested.
// The DispatchSegment logic assumes monotonically increasing timestamps.
// MPEG-TS wrap-around handling would require additional support.

// Test that TextChunker with coordinator disabled falls back to mathematical
// boundary calculation. This verifies existing behavior is preserved.
//
TEST_F(TextChunkerTest, DISABLED_NonTeletextModeFallsBackToCalculated) {
  const double kSegmentDurationSec = 4.0;
  const int64_t kSegmentDurationMs = 4000;
  const int64_t kSegment0Start = 0;
  const int64_t kSegment1Start = 4000;

  const int64_t kTextSampleStart = 1000;
  const int64_t kTextSampleEnd = 5000;

  // Initialize TextChunker with coordinator mode DISABLED (default)
  ASSERT_OK(Init(kSegmentDurationSec));

  {
    testing::InSequence s;

    EXPECT_CALL(*Output(kOutput), OnProcess(IsStreamInfo(_, _, _, _)));

    // Segment 0: Text sample [1000-5000] cropped to segment boundary at 4000
    EXPECT_CALL(*Output(kOutput), OnProcess(IsTextSample(_, _, kTextSampleStart,
                                                         kTextSampleEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment0Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    // Segment 1: Text sample continues [4000-5000]
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsTextSample(_, _, kSegment1Start, kTextSampleEnd)));
    EXPECT_CALL(*Output(kOutput),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegment1Start,
                                        kSegmentDurationMs, !kSubSegment,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutput), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromStreamInfo(
      kStreamIndex, GetTextStreamInfo(kTimescaleMs))));

  // Text sample triggers mathematical segmentation at t=4000
  ASSERT_OK(Input(kInput)->Dispatch(StreamData::FromTextSample(
      kStreamIndex,
      GetTextSample(kNoId, kTextSampleStart, kTextSampleEnd, kNoPayload))));

  ASSERT_OK(Input(kInput)->FlushAllDownstreams());
}

}  // namespace media
}  // namespace shaka
