// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/formats/webvtt/webvtt_to_mp4_handler.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;
const bool kEncrypted = true;

const size_t kInputCount = 1;
const size_t kOutputCount = 1;
const size_t kInputIndex = 0;
const size_t kOutputIndex = 0;

const char* kId[] = {"cue 1 id", "cue 2 id", "cue 3 id"};
const char* kPayload[] = {"cue 1 payload", "cue 2 payload", "cue 3 payload"};
const char* kNoSettings = "";

// These all refer to the samples. To make them easier to use in their
// correct context, they have purposely short names.
const size_t kA = 0;
const size_t kB = 1;
const size_t kC = 2;

}  // namespace

class TestableWebVttToMp4Handler : public WebVttToMp4Handler {
 public:
  MOCK_METHOD3(OnWriteCue,
               void(const std::string& id,
                    const std::string& settings,
                    const std::string& payload));

 protected:
  void WriteCue(const std::string& id,
                const std::string& settings,
                const std::string& payload,
                BufferWriter* out) {
    OnWriteCue(id, settings, payload);
    // We need to write something out or else media sample will think it is the
    // end of the stream.
    out->AppendInt(0);
  }
};

class WebVttToMp4HandlerTest : public MediaHandlerTestBase {
 protected:
  void SetUp() {
    mp4_handler_ = std::make_shared<TestableWebVttToMp4Handler>();
    ASSERT_OK(SetUpAndInitializeGraph(mp4_handler_, kInputCount, kOutputCount));
  }

  std::shared_ptr<TestableWebVttToMp4Handler> mp4_handler_;
};

// Verify that when the stream starts at a non-zero value, the gap at the
// start will be filled.
// |    [----A----]
TEST_F(WebVttToMp4HandlerTest, NonZeroStartTime) {
  const int64_t kGapStart = 0;
  const int64_t kGapEnd = 100;
  const int64_t kGapDuration = kGapEnd - kGapStart;

  const char* kSampleId = kId[0];
  const char* kSamplePayload = kPayload[0];
  const int64_t kSampleStart = kGapEnd;
  const int64_t kSampleDuration = 500;
  const int64_t kSampleEnd = kSampleStart + kSampleDuration;

  {
    testing::InSequence s;

    // Empty Cue to fill gap
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kGapStart, kGapDuration,
                                        !kEncrypted)));

    // Sample
    EXPECT_CALL(*mp4_handler_,
                OnWriteCue(kSampleId, kNoSettings, kSamplePayload));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kSampleStart,
                                        kSampleDuration, !kEncrypted)));

    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromTextSample(
                    kStreamIndex, GetTextSample(kSampleId, kSampleStart,
                                                kSampleEnd, kSamplePayload))));

  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());
}

// Verify the cues are grouped correctly when the cues do not overlap at all.
// An empty cue should be inserted between the two as there is a gap.
//
// [----A---]  [---B---]
TEST_F(WebVttToMp4HandlerTest, NoOverlap) {
  const int64_t kDuration = 1000;

  const char* kSample1Id = kId[0];
  const char* kSample1Payload = kPayload[0];
  const int64_t kSample1Start = 0;
  const int64_t kSample1End = kSample1Start + kDuration;

  // Make sample 2 be just a little after sample 1.
  const char* kSample2Id = kId[1];
  const char* kSample2Payload = kPayload[1];
  const int64_t kSample2Start = kSample1End + 100;
  const int64_t kSample2End = kSample2Start + kDuration;

  const int64_t kGapStart = kSample1End;
  const int64_t kGapDuration = kSample2Start - kSample1End;

  {
    testing::InSequence s;

    // Sample 1
    EXPECT_CALL(*mp4_handler_,
                OnWriteCue(kSample1Id, kNoSettings, kSample1Payload));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kSample1Start, kDuration,
                                        !kEncrypted)));

    // Empty Cue to fill gap
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kGapStart, kGapDuration,
                                        !kEncrypted)));

    // Sample 2
    EXPECT_CALL(*mp4_handler_,
                OnWriteCue(kSample2Id, kNoSettings, kSample2Payload));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kSample2Start, kDuration,
                                        !kEncrypted)));

    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(
      Input(kInputIndex)
          ->Dispatch(StreamData::FromTextSample(
              kStreamIndex, GetTextSample(kSample1Id, kSample1Start,
                                          kSample1End, kSample1Payload))));

  ASSERT_OK(
      Input(kInputIndex)
          ->Dispatch(StreamData::FromTextSample(
              kStreamIndex, GetTextSample(kSample2Id, kSample2Start,
                                          kSample2End, kSample2Payload))));

  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());
}

// Verify the cues are grouped correctly when one cue overlaps another cue at
// one end.
//
// [-------A-------]
//         [-------B------]
TEST_F(WebVttToMp4HandlerTest, Overlap) {
  const int64_t kStart[] = {0, 500};
  const int64_t kEnd[] = {1000, 1500};

  {
    testing::InSequence s;

    // Sample A
    EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[kA], kNoSettings, kPayload[kA]));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kStart[kA],
                                        kStart[kB] - kStart[kA], !kEncrypted)));

    // Sample A and B
    for (size_t i = kA; i <= kB; i++) {
      EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[i], kNoSettings, kPayload[i]));
    }
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kStart[kB],
                                        kEnd[kA] - kStart[kB], !kEncrypted)));

    // Sample B
    EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[kB], kNoSettings, kPayload[kB]));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kEnd[kA],
                                        kEnd[kB] - kEnd[kA], !kEncrypted)));

    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  for (size_t i = kA; i <= kB; i++) {
    ASSERT_OK(Input(kInputIndex)
                  ->Dispatch(StreamData::FromTextSample(
                      kStreamIndex,
                      GetTextSample(kId[i], kStart[i], kEnd[i], kPayload[i]))));
  }
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());
}

// Verify the cues are grouped correctly when one cue starts before and ends
// after another cue.
//
// [-------------A-------------]
//    [----------B----------]
TEST_F(WebVttToMp4HandlerTest, Contains) {
  const int64_t kStart[] = {0, 100};
  const int64_t kEnd[] = {1000, 900};

  {
    testing::InSequence s;

    // Sample A
    EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[kA], kNoSettings, kPayload[kA]));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kStart[kA],
                                        kStart[kB] - kStart[kA], !kEncrypted)));

    // Sample A and B
    for (size_t i = kA; i <= kB; i++) {
      EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[i], kNoSettings, kPayload[i]));
    }
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kStart[kB],
                                        kEnd[kB] - kStart[kB], !kEncrypted)));

    // Sample A
    EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[kA], kNoSettings, kPayload[kA]));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kEnd[kB],
                                        kEnd[kA] - kEnd[kB], !kEncrypted)));

    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  for (size_t i = kA; i <= kB; i++) {
    ASSERT_OK(Input(kInputIndex)
                  ->Dispatch(StreamData::FromTextSample(
                      kStreamIndex,
                      GetTextSample(kId[i], kStart[i], kEnd[i], kPayload[i]))));
  }
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());
}

// Verify that when two cues are completely on top of each other, that there
// is no extra boxes sent out.
//
// [----------A----------]
// [----------B----------]
TEST_F(WebVttToMp4HandlerTest, ExactOverlap) {
  const int64_t kStart = 0;
  const int64_t kDuration = 1000;
  const int64_t kEnd = kStart + kDuration;

  {
    testing::InSequence s;

    // Sample A and B
    for (size_t i = kA; i <= kB; i++) {
      EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[i], kNoSettings, kPayload[i]));
    }
    EXPECT_CALL(
        *Output(kOutputIndex),
        OnProcess(IsMediaSample(kStreamIndex, kStart, kDuration, !kEncrypted)));

    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  for (size_t i = kA; i <= kB; i++) {
    ASSERT_OK(Input(kInputIndex)
                  ->Dispatch(StreamData::FromTextSample(
                      kStreamIndex,
                      GetTextSample(kId[i], kStart, kEnd, kPayload[i]))));
  }
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());
}

// Verify that when two cues are completely on top of each other, that there
// is no extra boxes sent out.
//
// [----A----]
// [--------B--------]
// [------------C------------]
TEST_F(WebVttToMp4HandlerTest, OverlapStartWithStaggerEnd) {
  const int64_t kStart = 0;
  const int64_t kEnd[] = {1000, 2000, 3000};

  {
    testing::InSequence s;

    // Sample A, B, and C
    for (size_t i = kA; i <= kC; i++) {
      EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[i], kNoSettings, kPayload[i]));
    }
    EXPECT_CALL(
        *Output(kOutputIndex),
        OnProcess(IsMediaSample(kStreamIndex, kStart, kEnd[kA], !kEncrypted)));

    // Sample B and C
    for (size_t i = kB; i <= kC; i++) {
      EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[i], kNoSettings, kPayload[i]));
    }
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kEnd[kA],
                                        kEnd[kB] - kEnd[kA], !kEncrypted)));

    // Sample C
    EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[kC], kNoSettings, kPayload[kC]));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kEnd[kB],
                                        kEnd[kC] - kEnd[kB], !kEncrypted)));

    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  for (size_t i = kA; i <= kC; i++) {
    ASSERT_OK(Input(kInputIndex)
                  ->Dispatch(StreamData::FromTextSample(
                      kStreamIndex,
                      GetTextSample(kId[i], kStart, kEnd[i], kPayload[i]))));
  }
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());
}

// Verify that when two cues are completely on top of each other, that there
// is no extra boxes sent out.
//
// [------------A------------]
//         [--------B--------]
//                 [----C----]
TEST_F(WebVttToMp4HandlerTest, StaggerStartWithOverlapEnd) {
  const int64_t kStart[] = {0, 100, 200};
  const int64_t kEnd = 1000;

  {
    testing::InSequence s;

    // Sample A
    EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[kA], kNoSettings, kPayload[kA]));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kStart[kA],
                                        kStart[kB] - kStart[kA], !kEncrypted)));

    // Sample A and B
    for (size_t i = kA; i <= kB; i++) {
      EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[i], kNoSettings, kPayload[i]));
    }
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kStart[kB],
                                        kStart[kC] - kStart[kB], !kEncrypted)));

    // Sample A, B, and C
    for (size_t i = kA; i <= kC; i++) {
      EXPECT_CALL(*mp4_handler_, OnWriteCue(kId[i], kNoSettings, kPayload[i]));
    }
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsMediaSample(kStreamIndex, kStart[kC],
                                        kEnd - kStart[kC], !kEncrypted)));

    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  for (size_t i = kA; i <= kC; i++) {
    ASSERT_OK(Input(kInputIndex)
                  ->Dispatch(StreamData::FromTextSample(
                      kStreamIndex,
                      GetTextSample(kId[i], kStart[i], kEnd, kPayload[i]))));
  }
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());
}
}  // namespace media
}  // namespace shaka
