// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/webm/multi_segment_segmenter.h"

#include <gtest/gtest.h>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/formats/webm/segmenter_test_base.h"

namespace shaka {
namespace media {
namespace {

const uint64_t kDuration = 1000;

const uint8_t kBasicSupportDataInit[] = {
  // ID: EBML Header, Payload Size: 31
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
  // ID: Segment, Payload Size: Unknown
  0x18, 0x53, 0x80, 0x67, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    // ID: Void, Payload Size: 87
    0xec, 0xd7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    // ID: Info, Payload Size: 88
    0x15, 0x49, 0xa9, 0x66, 0xd8,
      // TimecodeScale: 1000000
      0x2a, 0xd7, 0xb1, 0x83, 0x0f, 0x42, 0x40,
      // Duration: float(0)
      0x44, 0x89, 0x84, 0x3f, 0x80, 0x00, 0x00,
      // MuxingApp: 'libwebm-0.2.1.0'
      0x4d, 0x80, 0x8f, 0x6c, 0x69, 0x62, 0x77, 0x65, 0x62, 0x6d, 0x2d, 0x30,
      0x2e, 0x32, 0x2e, 0x31, 0x2e, 0x30,
      // WritingApp: 'https://github.com/google/shaka-packager version test'
      0x57, 0x41, 0xb5,
      0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x67, 0x69, 0x74, 0x68,
      0x75, 0x62, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x67, 0x6f, 0x6f, 0x67, 0x6c,
      0x65, 0x2f, 0x73, 0x68, 0x61, 0x6b, 0x61, 0x2d, 0x70, 0x61, 0x63, 0x6b,
      0x61, 0x67, 0x65, 0x72, 0x20, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e,
      0x20, 0x74, 0x65, 0x73, 0x74,
    // ID: Tracks, Payload Size: 41
    0x16, 0x54, 0xae, 0x6b, 0xa9,
      // ID: Track, Payload Size: 39
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
        // ID: Video, Payload Size: 14
        0xe0, 0x8e,
          // PixelWidth: 100
          0xb0, 0x81, 0x64,
          // PixelHeight: 100
          0xba, 0x81, 0x64,
          // DisplayWidth: 100
          0x54, 0xb0, 0x81, 0x64,
          // DisplayHeight: 100
          0x54, 0xba, 0x81, 0x64
};
const uint8_t kBasicSupportDataSegment[] = {
  // ID: Cluster, Payload Size: 64
  0x1f, 0x43, 0xb6, 0x75, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    // Timecode: 0
    0xe7, 0x81, 0x00,
    // ID: SimpleBlock, Payload Size: 9
    0xa3, 0x89, 0x81, 0x00, 0x00, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
    // ID: SimpleBlock, Payload Size: 9
    0xa3, 0x89, 0x81, 0x03, 0xe8, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
    // ID: SimpleBlock, Payload Size: 9
    0xa3, 0x89, 0x81, 0x07, 0xd0, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
    // ID: SimpleBlock, Payload Size: 9
    0xa3, 0x89, 0x81, 0x0b, 0xb8, 0x80, 0xde, 0xad, 0xbe, 0xef, 0x00,
    // ID: BlockGroup, Payload Size: 15
    0xa0, 0x8f,
      // ID: Block, Payload Size: 9
      0xa1, 0x89, 0x81, 0x0f, 0xa0, 0x00, 0xde, 0xad, 0xbe, 0xef, 0x00,
      // BlockDuration: 1000
      0x9b, 0x82, 0x03, 0xe8
};

}  // namespace

class MultiSegmentSegmenterTest : public SegmentTestBase {
 public:
  MultiSegmentSegmenterTest() : info_(CreateVideoStreamInfo()) {}

 protected:
  void InitializeSegmenter(const MuxerOptions& options) {
    ASSERT_NO_FATAL_FAILURE(
        CreateAndInitializeSegmenter<webm::MultiSegmentSegmenter>(
            options, info_.get(), NULL, &segmenter_));
  }

  scoped_refptr<StreamInfo> info_;
  scoped_ptr<webm::Segmenter> segmenter_;
};

TEST_F(MultiSegmentSegmenterTest, BasicSupport) {
  MuxerOptions options = CreateMuxerOptions();
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 5; i++) {
    scoped_refptr<MediaSample> sample =
        CreateSample(kKeyFrame, kDuration, kNoSideData);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  // Verify the resulting data.
  ASSERT_FILE_EQ(OutputFileName().c_str(), kBasicSupportDataInit);
  ASSERT_FILE_EQ(TemplateFileName(0).c_str(), kBasicSupportDataSegment);

  // There is no second segment.
  EXPECT_FALSE(File::Open(TemplateFileName(1).c_str(), "r"));
}

TEST_F(MultiSegmentSegmenterTest, SplitsFilesOnSegmentDuration) {
  MuxerOptions options = CreateMuxerOptions();
  options.segment_duration = 5;  // seconds
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 8; i++) {
    scoped_refptr<MediaSample> sample =
        CreateSample(kKeyFrame, kDuration, kNoSideData);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  // Verify the resulting data.
  ClusterParser parser;
  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromCluster(TemplateFileName(0)));
  ASSERT_EQ(1, parser.cluster_count());
  EXPECT_EQ(5, parser.GetFrameCountForCluster(0));

  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromCluster(TemplateFileName(1)));
  ASSERT_EQ(1, parser.cluster_count());
  EXPECT_EQ(3, parser.GetFrameCountForCluster(0));

  EXPECT_FALSE(File::Open(TemplateFileName(2).c_str(), "r"));
}

TEST_F(MultiSegmentSegmenterTest, RespectsSegmentSAPAlign) {
  MuxerOptions options = CreateMuxerOptions();
  options.segment_duration = 3;  // seconds
  options.segment_sap_aligned = true;
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 10; i++) {
    const KeyFrameFlag key_frame_flag = i == 6 ? kKeyFrame : kNotKeyFrame;
    scoped_refptr<MediaSample> sample =
        CreateSample(key_frame_flag, kDuration, kNoSideData);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  // Verify the resulting data.
  ClusterParser parser;
  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromCluster(TemplateFileName(0)));
  ASSERT_EQ(1, parser.cluster_count());
  EXPECT_EQ(6, parser.GetFrameCountForCluster(0));

  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromCluster(TemplateFileName(1)));
  ASSERT_EQ(1, parser.cluster_count());
  EXPECT_EQ(4, parser.GetFrameCountForCluster(0));

  EXPECT_FALSE(File::Open(TemplateFileName(2).c_str(), "r"));
}

TEST_F(MultiSegmentSegmenterTest, SplitsClustersOnFragmentDuration) {
  MuxerOptions options = CreateMuxerOptions();
  options.fragment_duration = 5;  // seconds
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 8; i++) {
    scoped_refptr<MediaSample> sample =
        CreateSample(kKeyFrame, kDuration, kNoSideData);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  // Verify the resulting data.
  ClusterParser parser;
  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromCluster(TemplateFileName(0)));
  ASSERT_EQ(2, parser.cluster_count());
  EXPECT_EQ(5, parser.GetFrameCountForCluster(0));
  EXPECT_EQ(3, parser.GetFrameCountForCluster(1));

  EXPECT_FALSE(File::Open(TemplateFileName(1).c_str(), "r"));
}

TEST_F(MultiSegmentSegmenterTest, RespectsFragmentSAPAlign) {
  MuxerOptions options = CreateMuxerOptions();
  options.fragment_duration = 3;  // seconds
  options.fragment_sap_aligned = true;
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  for (int i = 0; i < 10; i++) {
    const KeyFrameFlag key_frame_flag = i == 6 ? kKeyFrame : kNotKeyFrame;
    scoped_refptr<MediaSample> sample =
        CreateSample(key_frame_flag, kDuration, kNoSideData);
    ASSERT_OK(segmenter_->AddSample(sample));
  }
  ASSERT_OK(segmenter_->Finalize());

  // Verify the resulting data.
  ClusterParser parser;
  ASSERT_NO_FATAL_FAILURE(parser.PopulateFromCluster(TemplateFileName(0)));
  ASSERT_EQ(2, parser.cluster_count());
  EXPECT_EQ(6, parser.GetFrameCountForCluster(0));
  EXPECT_EQ(4, parser.GetFrameCountForCluster(1));

  EXPECT_FALSE(File::Open(TemplateFileName(1).c_str(), "r"));
}

}  // namespace media
}  // namespace shaka

