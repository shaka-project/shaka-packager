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

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex0 = 0;
const size_t kStreamIndex1 = 1;
const uint32_t kTimeScale0 = 800;
const uint32_t kTimeScale1 = 1000;
const int64_t kDuration0 = 200;
const int64_t kDuration1 = 300;
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
      kStreamIndex0, GetAudioStreamInfo(kTimeScale0))));
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex0, kTimeScale0, !kEncrypted)));

  for (int i = 0; i < 5; ++i) {
    ClearOutputStreamDataVector();
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(i * kDuration1, kDuration1, kKeyFrame))));
    // One output stream_data except when i == 3, which also has SegmentInfo.
    if (i == 3) {
      EXPECT_THAT(GetOutputStreamDataVector(),
                  ElementsAre(IsSegmentInfo(kStreamIndex0, 0, kDuration1 * 3,
                                            !kIsSubsegment, !kEncrypted),
                              IsMediaSample(kStreamIndex0, i * kDuration1,
                                            kDuration1, !kEncrypted)));
    } else {
      EXPECT_THAT(GetOutputStreamDataVector(),
                  ElementsAre(IsMediaSample(kStreamIndex0, i * kDuration1,
                                            kDuration1, !kEncrypted)));
    }
  }

  ClearOutputStreamDataVector();
  ASSERT_OK(OnFlushRequest(kStreamIndex0));
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsSegmentInfo(kStreamIndex0, kDuration1 * 3, kDuration1 * 2,
                                !kIsSubsegment, !kEncrypted)));
}

TEST_F(ChunkingHandlerTest, AudioWithSubsegments) {
  ChunkingParams chunking_params;
  chunking_params.segment_duration_in_seconds = 1;
  chunking_params.subsegment_duration_in_seconds = 0.5;
  SetUpChunkingHandler(1, chunking_params);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex0, GetAudioStreamInfo(kTimeScale0))));
  for (int i = 0; i < 5; ++i) {
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(i * kDuration1, kDuration1, kKeyFrame))));
  }
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          IsStreamInfo(kStreamIndex0, kTimeScale0, !kEncrypted),
          IsMediaSample(kStreamIndex0, 0, kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kDuration1, kDuration1, !kEncrypted),
          IsSegmentInfo(kStreamIndex0, 0, kDuration1 * 2, kIsSubsegment,
                        !kEncrypted),
          IsMediaSample(kStreamIndex0, 2 * kDuration1, kDuration1, !kEncrypted),
          IsSegmentInfo(kStreamIndex0, 0, kDuration1 * 3, !kIsSubsegment,
                        !kEncrypted),
          IsMediaSample(kStreamIndex0, 3 * kDuration1, kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, 4 * kDuration1, kDuration1,
                        !kEncrypted)));
}

TEST_F(ChunkingHandlerTest, VideoAndSubsegmentAndNonzeroStart) {
  ChunkingParams chunking_params;
  chunking_params.segment_duration_in_seconds = 1;
  chunking_params.subsegment_duration_in_seconds = 0.3;
  SetUpChunkingHandler(1, chunking_params);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex0, GetVideoStreamInfo(kTimeScale1))));
  const int64_t kVideoStartTimestamp = 12345;
  for (int i = 0; i < 6; ++i) {
    // Alternate key frame.
    const bool is_key_frame = (i % 2) == 1;
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0,
        GetMediaSample(
            kVideoStartTimestamp + i * kDuration1,
            kDuration1,
            is_key_frame))));
  }
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          IsStreamInfo(kStreamIndex0, kTimeScale1, !kEncrypted),
          // The first samples @ kStartTimestamp is discarded - not key frame.
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration1,
                        kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration1 * 2,
                        kDuration1, !kEncrypted),
          // The next segment boundary 13245 / 1000 != 12645 / 1000.
          IsSegmentInfo(kStreamIndex0, kVideoStartTimestamp + kDuration1,
                        kDuration1 * 2, !kIsSubsegment, !kEncrypted),
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration1 * 3,
                        kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration1 * 4,
                        kDuration1, !kEncrypted),
          // The subsegment has duration kDuration1 * 2 since it can only
          // terminate before key frame.
          IsSegmentInfo(kStreamIndex0, kVideoStartTimestamp + kDuration1 * 3,
                        kDuration1 * 2, kIsSubsegment, !kEncrypted),
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration1 * 5,
                        kDuration1, !kEncrypted)));
}

TEST_F(ChunkingHandlerTest, AudioAndVideo) {
  ChunkingParams chunking_params;
  chunking_params.segment_duration_in_seconds = 1;
  chunking_params.subsegment_duration_in_seconds = 0.3;
  SetUpChunkingHandler(2, chunking_params);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex0, GetAudioStreamInfo(kTimeScale0))));
  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex1, GetVideoStreamInfo(kTimeScale1))));
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex0, kTimeScale0, !kEncrypted),
                  IsStreamInfo(kStreamIndex1, kTimeScale1, !kEncrypted)));
  ClearOutputStreamDataVector();

  // Equivalent to 12345 in video timescale.
  const int64_t kAudioStartTimestamp = 9876;
  const int64_t kVideoStartTimestamp = 12345;
  // Burst of audio and video samples. They will be properly ordered.
  for (int i = 0; i < 5; ++i) {
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0, GetMediaSample(kAudioStartTimestamp + kDuration0 * i,
                                      kDuration0, true))));
  }
  for (int i = 0; i < 5; ++i) {
    // Alternate key frame.
    const bool is_key_frame = (i % 2) == 1;
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex1, GetMediaSample(kVideoStartTimestamp + kDuration1 * i,
                                      kDuration1, is_key_frame))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // The first samples @ kStartTimestamp is discarded - not key frame.
          IsMediaSample(kStreamIndex1, kVideoStartTimestamp + kDuration1,
                        kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kAudioStartTimestamp + kDuration0,
                        kDuration0, !kEncrypted),
          IsMediaSample(kStreamIndex1, kVideoStartTimestamp + kDuration1 * 2,
                        kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kAudioStartTimestamp + kDuration0 * 2,
                        kDuration0, !kEncrypted),
          IsMediaSample(kStreamIndex0, kAudioStartTimestamp + kDuration0 * 3,
                        kDuration0, !kEncrypted),
          // The audio segment is terminated together with video stream.
          IsSegmentInfo(kStreamIndex0, kAudioStartTimestamp + kDuration0,
                        kDuration0 * 3, !kIsSubsegment, !kEncrypted),
          // The next segment boundary 13245 / 1000 != 12645 / 1000.
          IsSegmentInfo(kStreamIndex1, kVideoStartTimestamp + kDuration1,
                        kDuration1 * 2, !kIsSubsegment, !kEncrypted),
          IsMediaSample(kStreamIndex1, kVideoStartTimestamp + kDuration1 * 3,
                        kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kAudioStartTimestamp + kDuration0 * 4,
                        kDuration0, !kEncrypted)));
  ClearOutputStreamDataVector();

  // The side comments below show the equivalent timestamp in video timescale.
  // The audio and video are made ~aligned.
  ASSERT_OK(Process(StreamData::FromMediaSample(
      kStreamIndex0,
      GetMediaSample(
          kAudioStartTimestamp + kDuration0 * 5,
          kDuration0,
          true))));  // 13595
  ASSERT_OK(Process(StreamData::FromMediaSample(
      kStreamIndex1,
      GetMediaSample(
          kVideoStartTimestamp + kDuration1 * 5,
          kDuration1,
          true))));  // 13845
  ASSERT_OK(Process(StreamData::FromMediaSample(
      kStreamIndex0,
      GetMediaSample(
          kAudioStartTimestamp + kDuration0 * 6,
          kDuration0,
          true))));  // 13845
  // This expectation are separated from the expectation above because
  // ElementsAre supports at most 10 elements.
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          IsMediaSample(kStreamIndex1, kVideoStartTimestamp + kDuration1 * 4,
                        kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kAudioStartTimestamp + kDuration0 * 5,
                        kDuration0, !kEncrypted),
          // Audio is terminated along with video below.
          IsSegmentInfo(kStreamIndex0, kAudioStartTimestamp + kDuration0 * 4,
                        kDuration0 * 2, kIsSubsegment, !kEncrypted),
          // The subsegment has duration kDuration1 * 2 since it can only
          // terminate before key frame.
          IsSegmentInfo(kStreamIndex1, kVideoStartTimestamp + kDuration1 * 3,
                        kDuration1 * 2, kIsSubsegment, !kEncrypted),
          IsMediaSample(kStreamIndex1, kVideoStartTimestamp + kDuration1 * 5,
                        kDuration1, !kEncrypted)));

  ClearOutputStreamDataVector();
  ASSERT_OK(OnFlushRequest(kStreamIndex0));
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          IsMediaSample(kStreamIndex0, kAudioStartTimestamp + kDuration0 * 6,
                        kDuration0, !kEncrypted),
          IsSegmentInfo(kStreamIndex0, kAudioStartTimestamp + kDuration0 * 4,
                        kDuration0 * 3, !kIsSubsegment, !kEncrypted)));

  ClearOutputStreamDataVector();
  ASSERT_OK(OnFlushRequest(kStreamIndex1));
  EXPECT_THAT(GetOutputStreamDataVector(),
              ElementsAre(IsSegmentInfo(
                  kStreamIndex1, kVideoStartTimestamp + kDuration1 * 3,
                  kDuration1 * 3, !kIsSubsegment, !kEncrypted)));

  // Flush again will do nothing.
  ClearOutputStreamDataVector();
  ASSERT_OK(OnFlushRequest(kStreamIndex0));
  ASSERT_OK(OnFlushRequest(kStreamIndex1));
  EXPECT_THAT(GetOutputStreamDataVector(), IsEmpty());
}

TEST_F(ChunkingHandlerTest, Scte35Event) {
  ChunkingParams chunking_params;
  chunking_params.segment_duration_in_seconds = 1;
  chunking_params.subsegment_duration_in_seconds = 0.5;
  SetUpChunkingHandler(1, chunking_params);

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex0, GetVideoStreamInfo(kTimeScale1))));

  const int64_t kVideoStartTimestamp = 12345;

  auto scte35_event = std::make_shared<Scte35Event>();
  scte35_event->start_time = kVideoStartTimestamp + kDuration1;
  ASSERT_OK(Process(StreamData::FromScte35Event(kStreamIndex0, scte35_event)));

  for (int i = 0; i < 3; ++i) {
    const bool is_key_frame = true;
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex0, GetMediaSample(kVideoStartTimestamp + i * kDuration1,
                                      kDuration1, is_key_frame))));
  }
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          IsStreamInfo(kStreamIndex0, kTimeScale1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp, kDuration1,
                        !kEncrypted),
          // A new segment is created due to the existance of Cue.
          IsSegmentInfo(kStreamIndex0, kVideoStartTimestamp, kDuration1,
                        !kIsSubsegment, !kEncrypted),
          IsCueEvent(kStreamIndex0,
                     static_cast<double>(kVideoStartTimestamp + kDuration1)),
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration1 * 1,
                        kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kVideoStartTimestamp + kDuration1 * 2,
                        kDuration1, !kEncrypted)));
}

}  // namespace media
}  // namespace shaka
