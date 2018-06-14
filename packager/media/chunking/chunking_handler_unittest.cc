// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/chunking/chunking_handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler_test_base.h"
#include "packager/status_test_util.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;
const uint32_t kTimeScale0 = 800;
const uint32_t kTimeScale1 = 1000;
const int64_t kDuration = 300;
const bool kKeyFrame = true;
const bool kIsSubsegment = true;
const bool kEncrypted = true;

}  // namespace

class ChunkingHandlerTest : public MediaHandlerGraphTestBase {
 public:
  void SetUpChunkingHandler(int num_inputs,
                            const ChunkingParams& chunking_params) {
    chunking_handler_.reset(new ChunkingHandler(chunking_params));
    SetUpGraph(num_inputs, num_inputs, chunking_handler_);
    ASSERT_OK(chunking_handler_->Initialize());
  }

  Status Process(std::unique_ptr<StreamData> stream_data) {
    return chunking_handler_->Process(std::move(stream_data));
  }

  Status OnFlushRequest(int stream_index) {
    return chunking_handler_->OnFlushRequest(stream_index);
  }

 protected:
  std::shared_ptr<ChunkingHandler> chunking_handler_;
};

TEST_F(ChunkingHandlerTest, AudioNoSubsegmentsThenFlush) {
  ChunkingParams chunking_params;
  chunking_params.segment_duration_in_seconds = 1;
  SetUpChunkingHandler(1, chunking_params);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex, GetAudioStreamInfo(kTimeScale0))));
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex, kTimeScale0, !kEncrypted, _)));

  for (int i = 0; i < 5; ++i) {
    ClearOutputStreamDataVector();
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex, GetMediaSample(i * kDuration, kDuration, kKeyFrame))));
    // One output stream_data except when i == 3, which also has SegmentInfo.
    if (i == 3) {
      EXPECT_THAT(GetOutputStreamDataVector(),
                  ElementsAre(IsSegmentInfo(kStreamIndex, 0, kDuration * 3,
                                            !kIsSubsegment, !kEncrypted),
                              IsMediaSample(kStreamIndex, i * kDuration,
                                            kDuration, !kEncrypted, _)));
    } else {
      EXPECT_THAT(GetOutputStreamDataVector(),
                  ElementsAre(IsMediaSample(kStreamIndex, i * kDuration,
                                            kDuration, !kEncrypted, _)));
    }
  }

  ClearOutputStreamDataVector();
  ASSERT_OK(OnFlushRequest(kStreamIndex));
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsSegmentInfo(kStreamIndex, kDuration * 3, kDuration * 2,
                                !kIsSubsegment, !kEncrypted)));
}

TEST_F(ChunkingHandlerTest, AudioWithSubsegments) {
  ChunkingParams chunking_params;
  chunking_params.segment_duration_in_seconds = 1;
  chunking_params.subsegment_duration_in_seconds = 0.5;
  SetUpChunkingHandler(1, chunking_params);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex, GetAudioStreamInfo(kTimeScale0))));
  for (int i = 0; i < 5; ++i) {
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex, GetMediaSample(i * kDuration, kDuration, kKeyFrame))));
  }
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          IsStreamInfo(kStreamIndex, kTimeScale0, !kEncrypted, _),
          IsMediaSample(kStreamIndex, 0, kDuration, !kEncrypted, _),
          IsMediaSample(kStreamIndex, kDuration, kDuration, !kEncrypted, _),
          IsSegmentInfo(kStreamIndex, 0, kDuration * 2, kIsSubsegment,
                        !kEncrypted),
          IsMediaSample(kStreamIndex, 2 * kDuration, kDuration, !kEncrypted, _),
          IsSegmentInfo(kStreamIndex, 0, kDuration * 3, !kIsSubsegment,
                        !kEncrypted),
          IsMediaSample(kStreamIndex, 3 * kDuration, kDuration, !kEncrypted, _),
          IsMediaSample(kStreamIndex, 4 * kDuration, kDuration, !kEncrypted,
                        _)));
}

TEST_F(ChunkingHandlerTest, VideoAndSubsegmentAndNonzeroStart) {
  ChunkingParams chunking_params;
  chunking_params.segment_duration_in_seconds = 1;
  chunking_params.subsegment_duration_in_seconds = 0.3;
  SetUpChunkingHandler(1, chunking_params);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex, GetVideoStreamInfo(kTimeScale1))));
  const int64_t kVideoStartTimestamp = 12345;
  for (int i = 0; i < 6; ++i) {
    // Alternate key frame.
    const bool is_key_frame = (i % 2) == 1;
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex, GetMediaSample(kVideoStartTimestamp + i * kDuration,
                                     kDuration, is_key_frame))));
  }
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          IsStreamInfo(kStreamIndex, kTimeScale1, !kEncrypted, _),
          // The first samples @ kStartTimestamp is discarded - not key frame.
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration,
                        kDuration, !kEncrypted, _),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 2,
                        kDuration, !kEncrypted, _),
          // The next segment boundary 13245 / 1000 != 12645 / 1000.
          IsSegmentInfo(kStreamIndex, kVideoStartTimestamp + kDuration,
                        kDuration * 2, !kIsSubsegment, !kEncrypted),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 3,
                        kDuration, !kEncrypted, _),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 4,
                        kDuration, !kEncrypted, _),
          // The subsegment has duration kDuration * 2 since it can only
          // terminate before key frame.
          IsSegmentInfo(kStreamIndex, kVideoStartTimestamp + kDuration * 3,
                        kDuration * 2, kIsSubsegment, !kEncrypted),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 5,
                        kDuration, !kEncrypted, _)));
}

TEST_F(ChunkingHandlerTest, CueEvent) {
  ChunkingParams chunking_params;
  chunking_params.segment_duration_in_seconds = 1;
  chunking_params.subsegment_duration_in_seconds = 0.5;
  SetUpChunkingHandler(1, chunking_params);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex, GetVideoStreamInfo(kTimeScale1))));
  ClearOutputStreamDataVector();

  const int64_t kVideoStartTimestamp = 12345;
  const double kCueTimeInSeconds =
      static_cast<double>(kVideoStartTimestamp + kDuration) / kTimeScale1;

  auto cue_event = std::make_shared<CueEvent>();
  cue_event->time_in_seconds = kCueTimeInSeconds;

  for (int i = 0; i < 6; ++i) {
    const bool is_key_frame = true;
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex, GetMediaSample(kVideoStartTimestamp + i * kDuration,
                                     kDuration, is_key_frame))));
    if (i == 0) {
      ASSERT_OK(Process(StreamData::FromCueEvent(kStreamIndex, cue_event)));
    }
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          IsMediaSample(kStreamIndex, kVideoStartTimestamp, kDuration,
                        !kEncrypted, _),
          // A new segment is created due to the existance of Cue.
          IsSegmentInfo(kStreamIndex, kVideoStartTimestamp, kDuration,
                        !kIsSubsegment, !kEncrypted),
          IsCueEvent(kStreamIndex, kCueTimeInSeconds),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 1,
                        kDuration, !kEncrypted, _),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 2,
                        kDuration, !kEncrypted, _),
          IsSegmentInfo(kStreamIndex, kVideoStartTimestamp + kDuration,
                        kDuration * 2, kIsSubsegment, !kEncrypted),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 3,
                        kDuration, !kEncrypted, _),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 4,
                        kDuration, !kEncrypted, _),
          IsSegmentInfo(kStreamIndex, kVideoStartTimestamp + kDuration,
                        kDuration * 4, !kIsSubsegment, !kEncrypted),
          IsMediaSample(kStreamIndex, kVideoStartTimestamp + kDuration * 5,
                        kDuration, !kEncrypted, _)));
}

}  // namespace media
}  // namespace shaka
