// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/chunking/chunking_handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/base/test/status_test_util.h"

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

class ChunkingHandlerTest : public MediaHandlerTestBase {
 public:
  void SetUpChunkingHandler(int num_inputs,
                            const ChunkingOptions& chunking_options) {
    chunking_handler_.reset(new ChunkingHandler(chunking_options));
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
  ChunkingOptions chunking_options;
  chunking_options.segment_duration_in_seconds = 1;
  SetUpChunkingHandler(1, chunking_options);

  ASSERT_OK(Process(GetAudioStreamInfoStreamData(kStreamIndex0, kTimeScale0)));
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex0, kTimeScale0, !kEncrypted)));

  for (int i = 0; i < 5; ++i) {
    ClearOutputStreamDataVector();
    ASSERT_OK(Process(GetMediaSampleStreamData(kStreamIndex0, i * kDuration1,
                                               kDuration1, kKeyFrame)));
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
  ChunkingOptions chunking_options;
  chunking_options.segment_duration_in_seconds = 1;
  chunking_options.subsegment_duration_in_seconds = 0.5;
  SetUpChunkingHandler(1, chunking_options);

  ASSERT_OK(Process(GetAudioStreamInfoStreamData(kStreamIndex0, kTimeScale0)));
  for (int i = 0; i < 5; ++i) {
    ASSERT_OK(Process(GetMediaSampleStreamData(kStreamIndex0, i * kDuration1,
                                               kDuration1, kKeyFrame)));
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
  ChunkingOptions chunking_options;
  chunking_options.segment_duration_in_seconds = 1;
  chunking_options.subsegment_duration_in_seconds = 0.3;
  SetUpChunkingHandler(1, chunking_options);

  ASSERT_OK(Process(GetVideoStreamInfoStreamData(kStreamIndex0, kTimeScale1)));
  const int64_t kVideoStartTimestamp = 12345;
  for (int i = 0; i < 6; ++i) {
    // Alternate key frame.
    const bool is_key_frame = (i % 2) == 1;
    ASSERT_OK(Process(GetMediaSampleStreamData(
        kStreamIndex0, kVideoStartTimestamp + i * kDuration1, kDuration1,
        is_key_frame)));
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
  ChunkingOptions chunking_options;
  chunking_options.segment_duration_in_seconds = 1;
  chunking_options.subsegment_duration_in_seconds = 0.3;
  SetUpChunkingHandler(2, chunking_options);

  ASSERT_OK(Process(GetAudioStreamInfoStreamData(kStreamIndex0, kTimeScale0)));
  ASSERT_OK(Process(GetVideoStreamInfoStreamData(kStreamIndex1, kTimeScale1)));
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex0, kTimeScale0, !kEncrypted),
                  IsStreamInfo(kStreamIndex1, kTimeScale1, !kEncrypted)));
  ClearOutputStreamDataVector();

  // Equivalent to 12345 in video timescale.
  const int64_t kAudioStartTimestamp = 9876;
  const int64_t kVideoStartTimestamp = 12345;
  for (int i = 0; i < 5; ++i) {
    ASSERT_OK(Process(GetMediaSampleStreamData(
        kStreamIndex0, kAudioStartTimestamp + kDuration0 * i, kDuration0,
        true)));
    // Alternate key frame.
    const bool is_key_frame = (i % 2) == 1;
    ASSERT_OK(Process(GetMediaSampleStreamData(
        kStreamIndex1, kVideoStartTimestamp + kDuration1 * i, kDuration1,
        is_key_frame)));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
          // The first samples @ kStartTimestamp is discarded - not key frame.
          IsMediaSample(kStreamIndex0, kAudioStartTimestamp + kDuration0,
                        kDuration0, !kEncrypted),
          IsMediaSample(kStreamIndex1, kVideoStartTimestamp + kDuration1,
                        kDuration1, !kEncrypted),
          IsMediaSample(kStreamIndex0, kAudioStartTimestamp + kDuration0 * 2,
                        kDuration0, !kEncrypted),
          IsMediaSample(kStreamIndex1, kVideoStartTimestamp + kDuration1 * 2,
                        kDuration1, !kEncrypted),
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
                        kDuration0, !kEncrypted),
          IsMediaSample(kStreamIndex1, kVideoStartTimestamp + kDuration1 * 4,
                        kDuration1, !kEncrypted)));
  ClearOutputStreamDataVector();

  // The side comments below show the equivalent timestamp in video timescale.
  // The audio and video are made ~aligned.
  ASSERT_OK(Process(GetMediaSampleStreamData(
      kStreamIndex0, kAudioStartTimestamp + kDuration0 * 5, kDuration0,
      true)));  // 13595
  ASSERT_OK(Process(GetMediaSampleStreamData(
      kStreamIndex1, kVideoStartTimestamp + kDuration1 * 5, kDuration1,
      true)));  // 13845
  ASSERT_OK(Process(GetMediaSampleStreamData(
      kStreamIndex0, kAudioStartTimestamp + kDuration0 * 6, kDuration0,
      true)));  // 13845
  // This expectation are separated from the expectation above because
  // ElementsAre supports at most 10 elements.
  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(
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

}  // namespace media
}  // namespace shaka
