// Copyright 2025 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/chunking/segment_coordinator.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/macros/status.h>
#include <packager/media/base/media_handler_test_base.h>
#include <packager/status/status_test_util.h>

using ::testing::_;

namespace shaka {
namespace media {
namespace {

const size_t kThreeInputs = 3;
const size_t kThreeOutputs = 3;

const int64_t kTimescale = 90000;

const size_t kStreamIndex = 0;

// Stream indices for different stream types
const size_t kVideoStreamIndex = 0;
const size_t kAudioStreamIndex = 1;
const size_t kTeletextStreamIndex = 2;

}  // namespace

class SegmentCoordinatorTest : public MediaHandlerTestBase {
 protected:
  void SetUpCoordinator(size_t num_inputs, size_t num_outputs) {
    coordinator_ = std::make_shared<SegmentCoordinator>();
    ASSERT_OK(SetUpAndInitializeGraph(coordinator_, num_inputs, num_outputs));
  }

  void MarkAsTeletext(size_t stream_index) {
    coordinator_->MarkAsTeletextStream(stream_index);
  }

  Status DispatchStreamInfo(size_t input_index, StreamType stream_type) {
    std::shared_ptr<StreamInfo> info;
    if (stream_type == StreamType::kStreamVideo) {
      info = GetVideoStreamInfo(kTimescale);
    } else if (stream_type == StreamType::kStreamAudio) {
      info = GetAudioStreamInfo(kTimescale);
    } else {
      info = GetTextStreamInfo(kTimescale);
    }
    auto data = StreamData::FromStreamInfo(kStreamIndex, std::move(info));
    return Input(input_index)->Dispatch(std::move(data));
  }

  Status DispatchSegmentInfo(size_t input_index,
                             int64_t start_time,
                             int64_t duration,
                             int64_t segment_number) {
    auto info = std::make_shared<SegmentInfo>();
    info->start_timestamp = start_time;
    info->duration = duration;
    info->segment_number = segment_number;
    info->is_subsegment = false;
    info->is_encrypted = false;

    auto data = StreamData::FromSegmentInfo(kStreamIndex, std::move(info));
    return Input(input_index)->Dispatch(std::move(data));
  }

  Status DispatchSubsegmentInfo(size_t input_index,
                                int64_t start_time,
                                int64_t duration,
                                int64_t segment_number) {
    auto info = std::make_shared<SegmentInfo>();
    info->start_timestamp = start_time;
    info->duration = duration;
    info->segment_number = segment_number;
    info->is_subsegment = true;
    info->is_encrypted = false;

    auto data = StreamData::FromSegmentInfo(kStreamIndex, std::move(info));
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

  std::shared_ptr<SegmentCoordinator> coordinator_;
};

// Test 1: ReceivesAndBroadcastsSegmentInfoToTeletext
// Verify that SegmentInfo from video stream is replicated to registered
// teletext streams only.
TEST_F(SegmentCoordinatorTest, ReceivesAndBroadcastsSegmentInfoToTeletext) {
  SetUpCoordinator(kThreeInputs, kThreeOutputs);
  MarkAsTeletext(kTeletextStreamIndex);

  const int64_t kSegmentStart = 0;
  const int64_t kSegmentDuration = 4000;
  const int64_t kSegmentNumber = 1;

  {
    testing::InSequence s;

    // Video stream info passes through
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));

    // Audio stream info passes through
    EXPECT_CALL(*Output(kAudioStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));

    // Teletext stream info passes through
    EXPECT_CALL(*Output(kTeletextStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));

    // Video segment info passes through to video output
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart,
                                        kSegmentDuration, _, _)));

    // Video segment info is replicated to teletext output
    EXPECT_CALL(*Output(kTeletextStreamIndex),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart,
                                        kSegmentDuration, _, _)));

    // Flush all streams
    EXPECT_CALL(*Output(kVideoStreamIndex), OnFlush(kStreamIndex));
    EXPECT_CALL(*Output(kAudioStreamIndex), OnFlush(kStreamIndex));
    EXPECT_CALL(*Output(kTeletextStreamIndex), OnFlush(kStreamIndex));
  }

  // Dispatch stream info for all streams
  ASSERT_OK(DispatchStreamInfo(kVideoStreamIndex, StreamType::kStreamVideo));
  ASSERT_OK(DispatchStreamInfo(kAudioStreamIndex, StreamType::kStreamAudio));
  ASSERT_OK(DispatchStreamInfo(kTeletextStreamIndex, StreamType::kStreamText));

  // Video stream dispatches SegmentInfo
  ASSERT_OK(DispatchSegmentInfo(kVideoStreamIndex, kSegmentStart,
                                kSegmentDuration, kSegmentNumber));

  // Flush all streams
  ASSERT_OK(
      FlushAll({kVideoStreamIndex, kAudioStreamIndex, kTeletextStreamIndex}));
}

// Test 2: HandlesMultipleTeletextStreams
// Verify that SegmentInfo is replicated to all registered teletext streams.
TEST_F(SegmentCoordinatorTest, HandlesMultipleTeletextStreams) {
  // Setup: 3 inputs (video, teletext1, teletext2), 3 outputs
  SetUpCoordinator(kThreeInputs, kThreeOutputs);

  const size_t kTeletext1Index = 1;
  const size_t kTeletext2Index = 2;

  MarkAsTeletext(kTeletext1Index);
  MarkAsTeletext(kTeletext2Index);

  const int64_t kSegmentStart = 0;
  const int64_t kSegmentDuration = 4000;
  const int64_t kSegmentNumber = 1;

  {
    testing::InSequence s;

    // Stream info for all streams passes through
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));
    EXPECT_CALL(*Output(kTeletext1Index),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));
    EXPECT_CALL(*Output(kTeletext2Index),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));

    // Video segment info passes through to video output
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart,
                                        kSegmentDuration, _, _)));

    // Video segment info is replicated to both teletext streams
    EXPECT_CALL(*Output(kTeletext1Index),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart,
                                        kSegmentDuration, _, _)));
    EXPECT_CALL(*Output(kTeletext2Index),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart,
                                        kSegmentDuration, _, _)));

    // Flush all streams
    EXPECT_CALL(*Output(kVideoStreamIndex), OnFlush(kStreamIndex));
    EXPECT_CALL(*Output(kTeletext1Index), OnFlush(kStreamIndex));
    EXPECT_CALL(*Output(kTeletext2Index), OnFlush(kStreamIndex));
  }

  // Dispatch stream info
  ASSERT_OK(DispatchStreamInfo(kVideoStreamIndex, StreamType::kStreamVideo));
  ASSERT_OK(DispatchStreamInfo(kTeletext1Index, StreamType::kStreamText));
  ASSERT_OK(DispatchStreamInfo(kTeletext2Index, StreamType::kStreamText));

  // Video stream dispatches SegmentInfo - should replicate to both teletext
  ASSERT_OK(DispatchSegmentInfo(kVideoStreamIndex, kSegmentStart,
                                kSegmentDuration, kSegmentNumber));

  ASSERT_OK(FlushAll({kVideoStreamIndex, kTeletext1Index, kTeletext2Index}));
}

// Test 3: IgnoresNonSegmentData
// Verify that non-SegmentInfo data types pass through unchanged.
TEST_F(SegmentCoordinatorTest, IgnoresNonSegmentData) {
  SetUpCoordinator(kThreeInputs, kThreeOutputs);
  MarkAsTeletext(kTeletextStreamIndex);

  const int64_t kTextSampleStart = 1000;
  const int64_t kTextSampleEnd = 5000;

  {
    testing::InSequence s;

    // Stream info passes through
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));
    EXPECT_CALL(*Output(kTeletextStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));

    // Text sample from teletext stream passes through unchanged
    EXPECT_CALL(*Output(kTeletextStreamIndex),
                OnProcess(IsTextSample(kStreamIndex, _, kTextSampleStart,
                                       kTextSampleEnd)));

    // Flush
    EXPECT_CALL(*Output(kVideoStreamIndex), OnFlush(kStreamIndex));
    EXPECT_CALL(*Output(kTeletextStreamIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(DispatchStreamInfo(kVideoStreamIndex, StreamType::kStreamVideo));
  ASSERT_OK(DispatchStreamInfo(kTeletextStreamIndex, StreamType::kStreamText));

  // Dispatch text sample - should pass through without replication
  ASSERT_OK(DispatchTextSample(kTeletextStreamIndex, kTextSampleStart,
                               kTextSampleEnd));

  ASSERT_OK(FlushAll({kVideoStreamIndex, kTeletextStreamIndex}));
}

// Test 4: OnlyBroadcastsToRegisteredStreams
// Verify that SegmentInfo is NOT replicated to non-registered streams.
TEST_F(SegmentCoordinatorTest, OnlyBroadcastsToRegisteredStreams) {
  SetUpCoordinator(kThreeInputs, kThreeOutputs);

  // Only mark kTeletextStreamIndex as teletext
  // kAudioStreamIndex is NOT marked as teletext
  MarkAsTeletext(kTeletextStreamIndex);

  const int64_t kSegmentStart = 0;
  const int64_t kSegmentDuration = 4000;
  const int64_t kSegmentNumber = 1;

  {
    testing::InSequence s;

    // Stream info for all streams
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));
    EXPECT_CALL(*Output(kAudioStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));
    EXPECT_CALL(*Output(kTeletextStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));

    // Video segment info passes through to video output
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart,
                                        kSegmentDuration, _, _)));

    // Video segment info is replicated ONLY to registered teletext stream
    // Audio stream (not registered) does NOT receive replication
    EXPECT_CALL(*Output(kTeletextStreamIndex),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart,
                                        kSegmentDuration, _, _)));

    // Flush
    EXPECT_CALL(*Output(kVideoStreamIndex), OnFlush(kStreamIndex));
    EXPECT_CALL(*Output(kAudioStreamIndex), OnFlush(kStreamIndex));
    EXPECT_CALL(*Output(kTeletextStreamIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(DispatchStreamInfo(kVideoStreamIndex, StreamType::kStreamVideo));
  ASSERT_OK(DispatchStreamInfo(kAudioStreamIndex, StreamType::kStreamAudio));
  ASSERT_OK(DispatchStreamInfo(kTeletextStreamIndex, StreamType::kStreamText));

  // Video stream dispatches SegmentInfo
  // Should replicate to teletext but NOT to audio
  ASSERT_OK(DispatchSegmentInfo(kVideoStreamIndex, kSegmentStart,
                                kSegmentDuration, kSegmentNumber));

  ASSERT_OK(
      FlushAll({kVideoStreamIndex, kAudioStreamIndex, kTeletextStreamIndex}));
}

// Additional test: Verify subsegments are not replicated
TEST_F(SegmentCoordinatorTest, SubsegmentsNotReplicated) {
  SetUpCoordinator(kThreeInputs, kThreeOutputs);
  MarkAsTeletext(kTeletextStreamIndex);

  const int64_t kSegmentStart = 0;
  const int64_t kSegmentDuration = 1000;
  const int64_t kSegmentNumber = 1;

  {
    testing::InSequence s;

    // Stream info
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));
    EXPECT_CALL(*Output(kTeletextStreamIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimescale, _, _)));

    // Subsegment info passes through to video output only
    // NOT replicated to teletext
    EXPECT_CALL(*Output(kVideoStreamIndex),
                OnProcess(IsSegmentInfo(kStreamIndex, kSegmentStart,
                                        kSegmentDuration, _, _)));

    // Flush
    EXPECT_CALL(*Output(kVideoStreamIndex), OnFlush(kStreamIndex));
    EXPECT_CALL(*Output(kTeletextStreamIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(DispatchStreamInfo(kVideoStreamIndex, StreamType::kStreamVideo));
  ASSERT_OK(DispatchStreamInfo(kTeletextStreamIndex, StreamType::kStreamText));

  // Dispatch subsegment - should NOT be replicated
  ASSERT_OK(DispatchSubsegmentInfo(kVideoStreamIndex, kSegmentStart,
                                   kSegmentDuration, kSegmentNumber));

  ASSERT_OK(FlushAll({kVideoStreamIndex, kTeletextStreamIndex}));
}

}  // namespace media
}  // namespace shaka
