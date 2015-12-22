// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/webm/single_segment_segmenter.h"
#include "packager/media/formats/webm/two_pass_single_segment_segmenter.h"

#include <gtest/gtest.h>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/formats/webm/segmenter_test_base.h"

namespace edash_packager {
namespace media {
namespace {

const uint64_t kDuration = 1000;

const uint8_t kBasicSupportData[] = {
  // ID: EBML Header, Size: 31
  0x1a, 0x45, 0xdf, 0xa3, 0x9f,
    // EBMLVersion: 1
    0x42, 0x86, 0x81, 0x01,
    // EBMLReadVersion: 1
    0x42, 0xf7, 0x81, 0x01,
    // EBMLMaxIDLength: 4
    0x42, 0xf2, 0x81, 0x04,
    // EBMLMaxSizeLength: 8
    0x42, 0xf3, 0x81, 0x08,
    // DocType: 'webm'
    0x42, 0x82, 0x84, 0x77, 0x65, 0x62, 0x6d,
    // DocTypeVersion: 2
    0x42, 0x87, 0x81, 0x02,
    // DocTypeReadVersion: 2
    0x42, 0x85, 0x81, 0x02,
  // ID: Segment, Size: 287
  0x18, 0x53, 0x80, 0x67, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x16,
    // ID: SeekHead, Size: 29
    0x11, 0x4d, 0x9b, 0x74, 0x9d,
      // ID: Seek, Size: 11
      0x4d, 0xbb, 0x8b,
        // SeekID: binary(4)
        0x53, 0xab, 0x84, 0x1f, 0x43, 0xb6, 0x75,
        // SeekPosition: 238
        0x53, 0xac, 0x81, 0xee,
      // ID: Seek, Size: 12
      0x4d, 0xbb, 0x8c,
        // SeekID: binary(4)
        0x53, 0xab, 0x84, 0x1c, 0x53, 0xbb, 0x6b,
        // SeekPosition: 174
        0x53, 0xac, 0x82, 0x01, 0x34,
    // ID: Void, Size: 53
    0xec, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ID: Info, size: 50
    0x15, 0x49, 0xa9, 0x66, 0xb2,
      // TimecodeScale: 1000000
      0x2a, 0xd7, 0xb1, 0x83, 0x0f, 0x42, 0x40,
      // Duration: float(5000)
      0x44, 0x89, 0x84, 0x45, 0x9c, 0x40, 0x00,
      // MuxingApp: 'libwebm-0.2.1.0'
      0x4d, 0x80, 0x8f, 0x6c, 0x69, 0x62, 0x77, 0x65, 0x62, 0x6d, 0x2d, 0x30,
      0x2e, 0x32, 0x2e, 0x31, 0x2e, 0x30,
      // WritingApp: 'libwebm-0.2.1.0'
      0x57, 0x41, 0x8f, 0x6c, 0x69, 0x62, 0x77, 0x65, 0x62, 0x6d, 0x2d, 0x30,
      0x2e, 0x32, 0x2e, 0x31, 0x2e, 0x30,
    // ID: Tracks, size: 41
    0x16, 0x54, 0xae, 0x6b, 0xa9,
      // ID: Track, size: 39
      0xae, 0xa7,
        // TrackNumber: 1
        0xd7, 0x81, 0x01,
        // TrackUID: 1
        0x73, 0xc5, 0x81, 0x01,
        // TrackType: 1
        0x83, 0x81, 0x01,
        // CodecID: 'V_VP8'
        0x86, 0x85, 0x56, 0x5f, 0x56, 0x50, 0x38,
        // Language: 'en'
        0x22, 0xb5, 0x9c, 0x82, 0x65, 0x6e,
        // ID: Video, Size: 14
        0xe0, 0x8e,
          // PixelWidth: 100
          0xb0, 0x81, 0x64,
          // PixelHeight: 100
          0xba, 0x81, 0x64,
          // DisplayWidth: 100
          0x54, 0xb0, 0x81, 0x64,
          // DisplayHeight: 100
          0x54, 0xba, 0x81, 0x64,
    // ID: Cluster, size: 58
    0x1f, 0x43, 0xb6, 0x75, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a,
      // Timecode: 0
      0xe7, 0x81, 0x00,
      // ID: SimpleBlock, Size: 9
      0xa3, 0x89, 0x81, 0x00, 0x00, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
      // ID: SimpleBlock, Size: 9
      0xa3, 0x89, 0x81, 0x03, 0xe8, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
      // ID: SimpleBlock, Size: 9
      0xa3, 0x89, 0x81, 0x07, 0xd0, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
      // ID: SimpleBlock, Size: 9
      0xa3, 0x89, 0x81, 0x0b, 0xb8, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
      // ID: SimpleBlock, Size: 9
      0xa3, 0x89, 0x81, 0x0f, 0xa0, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
    // ID: Cues, Size: 13
    0x1c, 0x53, 0xbb, 0x6b, 0x8d,
      // ID: CuePoint, Size: 11
      0xbb, 0x8b,
        // CueTime: 0
        0xb3, 0x81, 0x00,
        // ID: CueTrackPositions, Size: 6
        0xb7, 0x86,
          // CueTrack: 1
          0xf7, 0x81, 0x01,
          // CueClusterPosition: 190
          0xf1, 0x81, 0xbe
};

}  // namespace

// This is a parameterized test that tests both SingleSegmentSegmenter and
// TwoPassSingleSegmentSegmenter, since they should provide the exact same
// output.
class SingleSegmentSegmenterTest : public SegmentTestBase,
                                   public ::testing::WithParamInterface<bool> {
 public:
  SingleSegmentSegmenterTest() : info_(CreateVideoStreamInfo()) {}

 protected:
  void InitializeSegmenter(const MuxerOptions& options) {
    if (!GetParam()) {
      ASSERT_NO_FATAL_FAILURE(
          CreateAndInitializeSegmenter<webm::SingleSegmentSegmenter>(
              options, info_.get(), &segmenter_));
    } else {
      ASSERT_NO_FATAL_FAILURE(
          CreateAndInitializeSegmenter<webm::TwoPassSingleSegmentSegmenter>(
              options, info_.get(), &segmenter_));
    }
  }

  scoped_refptr<StreamInfo> info_;
  scoped_ptr<webm::Segmenter> segmenter_;
};

TEST_P(SingleSegmentSegmenterTest, BasicSupport) {
  MuxerOptions options = CreateMuxerOptions();
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 5; i++) {
    scoped_refptr<MediaSample> sample = CreateSample(true, kDuration);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  ASSERT_FILE_EQ(OutputFileName().c_str(), kBasicSupportData);
}

TEST_P(SingleSegmentSegmenterTest, SplitsClustersOnSegmentDuration) {
  MuxerOptions options = CreateMuxerOptions();
  options.segment_duration = 4.5;  // seconds
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 8; i++) {
    scoped_refptr<MediaSample> sample = CreateSample(true, kDuration);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  // Verify the resulting data.
  ClusterParser parser;
  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromSegment(OutputFileName()));
  ASSERT_EQ(2, parser.cluster_count());
  EXPECT_EQ(5, parser.GetFrameCountForCluster(0));
  EXPECT_EQ(3, parser.GetFrameCountForCluster(1));
}

TEST_P(SingleSegmentSegmenterTest, IgnoresFragmentDuration) {
  MuxerOptions options = CreateMuxerOptions();
  options.fragment_duration = 5;  // seconds
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 8; i++) {
    scoped_refptr<MediaSample> sample = CreateSample(true, kDuration);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  // Verify the resulting data.
  ClusterParser parser;
  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromSegment(OutputFileName()));
  ASSERT_EQ(1, parser.cluster_count());
  EXPECT_EQ(8, parser.GetFrameCountForCluster(0));
}

TEST_P(SingleSegmentSegmenterTest, RespectsSAPAlign) {
  MuxerOptions options = CreateMuxerOptions();
  options.segment_duration = 3;  // seconds
  options.segment_sap_aligned = true;
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 10; i++) {
    scoped_refptr<MediaSample> sample = CreateSample(i == 6, kDuration);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  // Verify the resulting data.
  ClusterParser parser;
  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromSegment(OutputFileName()));
  // Segments are 1 second, so there would normally be 3 frames per cluster,
  // but since it's SAP aligned and only frame 7 is a key-frame, there are
  // two clusters with 6 and 4 frames respectively.
  ASSERT_EQ(2, parser.cluster_count());
  EXPECT_EQ(6, parser.GetFrameCountForCluster(0));
  EXPECT_EQ(4, parser.GetFrameCountForCluster(1));
}

INSTANTIATE_TEST_CASE_P(TrueIsTwoPass,
                        SingleSegmentSegmenterTest,
                        ::testing::Bool());

}  // namespace media
}  // namespace edash_packager

