// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/webm/two_pass_single_segment_segmenter.h"

#include <gtest/gtest.h>
#include <memory>
#include "packager/media/formats/webm/segmenter_test_base.h"

namespace shaka {
namespace media {
namespace {

const int32_t kTimeScale = 1000000;
const int64_t kDuration = 1000000;
const bool kSubsegment = true;
const uint8_t kPerSampleIvSize = 8u;
const uint8_t kKeyId[] = {
    0x4c, 0x6f, 0x72, 0x65, 0x6d, 0x20, 0x69, 0x70,
    0x73, 0x75, 0x6d, 0x20, 0x64, 0x6f, 0x6c, 0x6f,
};
const uint8_t kIv[] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45,
};
const uint8_t kBasicSupportData[] = {
  // ID: EBML Header omitted.
  // ID: Segment, Payload Size: 432
  0x18, 0x53, 0x80, 0x67, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xb0,
    // ID: SeekHead, Payload Size: 58
    0x11, 0x4d, 0x9b, 0x74, 0xba,
      // ID: Seek, Payload Size: 11
      0x4d, 0xbb, 0x8b,
        // SeekID: binary(4) (Info)
        0x53, 0xab, 0x84, 0x15, 0x49, 0xa9, 0x66,
        // SeekPosition: 89
        0x53, 0xac, 0x81, 0x59,
      // ID: Seek, Payload Size: 11
      0x4d, 0xbb, 0x8b,
        // SeekID: binary(4) (Tracks)
        0x53, 0xab, 0x84, 0x16, 0x54, 0xae, 0x6b,
        // SeekPosition: 182
        0x53, 0xac, 0x81, 0xb6,
      // ID: Seek, Payload Size: 12
      0x4d, 0xbb, 0x8c,
        // SeekID: binary(4) (Cues)
        0x53, 0xab, 0x84, 0x1c, 0x53, 0xbb, 0x6b,
        // SeekPosition: 279
        0x53, 0xac, 0x82, 0x01, 0x17,
      // ID: Seek, Payload Size: 12
      0x4d, 0xbb, 0x8c,
        // SeekID: binary(4) (Cluster)
        0x53, 0xab, 0x84, 0x1f, 0x43, 0xb6, 0x75,
        // SeekPosition: 313
        0x53, 0xac, 0x82, 0x01, 0x39,
    // ID: Void, Payload Size: 24
    0xec, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    // ID: Info, Payload Size: 88
    0x15, 0x49, 0xa9, 0x66, 0xd8,
      // TimecodeScale: 1000000
      0x2a, 0xd7, 0xb1, 0x83, 0x0f, 0x42, 0x40,
      // Duration: float(5000)
      0x44, 0x89, 0x84, 0x45, 0x9c, 0x40, 0x00,
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
    // ID: Tracks, Payload Size: 92
    0x16, 0x54, 0xae, 0x6b, 0xdc,
      // ID: Track, Payload Size: 90
      0xae, 0xda,
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
        // ID: ContentEncodings, Payload Size: 48
        0x6d, 0x80, 0xb0,
          // ID: ContentEncoding, Payload Size: 45
          0x62, 0x40, 0xad,
            // ContentEncodingOrder: 0
            0x50, 0x31, 0x81, 0x00,
            // ContentEncodingScope: 1
            0x50, 0x32, 0x81, 0x01,
            // ContentEncodingType: 1
            0x50, 0x33, 0x81, 0x01,
            // ID: ContentEncryption, Payload Size: 30
            0x50, 0x35, 0x9e,
              // ContentEncAlgo: 5
              0x47, 0xe1, 0x81, 0x05,
              // ContentEncKeyID: binary(16)
              0x47, 0xe2, 0x90,
                0x4c, 0x6f, 0x72, 0x65, 0x6d, 0x20, 0x69, 0x70,
                0x73, 0x75, 0x6d, 0x20, 0x64, 0x6f, 0x6c, 0x6f,
              // ID: ContentEncAESSettings, Payload Size: 4
              0x47, 0xe7, 0x84,
                // AESSettingsCipherMode: 1
                0x47, 0xe8, 0x81, 0x01,
        // ID: Video, Payload Size: 14
        0xe0, 0x8e,
          // PixelWidth: 100
          0xb0, 0x81, 0x64,
          // PixelHeight: 100
          0xba, 0x81, 0x64,
          // DisplayWidth: 100
          0x54, 0xb0, 0x81, 0x64,
          // DisplayHeight: 100
          0x54, 0xba, 0x81, 0x64,
    // ID: Cues, Payload Size: 29
    0x1c, 0x53, 0xbb, 0x6b, 0x9d,
      // ID: CuePoint, Payload Size: 12
      0xbb, 0x8c,
        // CueTime: 0
        0xb3, 0x81, 0x00,
        // ID: CueTrackPositions, Payload Size: 7
        0xb7, 0x87,
          // CueTrack: 1
          0xf7, 0x81, 0x01,
          // CueClusterPosition: 313
          0xf1, 0x82, 0x01, 0x39,
      // ID: CuePoint, Payload Size: 13
      0xbb, 0x8d,
        // CueTime: 3000
        0xb3, 0x82, 0x0b, 0xb8,
        // ID: CueTrackPositions, Payload Size: 7
        0xb7, 0x87,
          // CueTrack: 1
          0xf7, 0x81, 0x01,
          // CueClusterPosition: 370
          0xf1, 0x82, 0x01, 0x72,
    // ID: Cluster, Payload Size: 45
    0x1f, 0x43, 0xb6, 0x75, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d,
      // Timecode: 0
      0xe7, 0x81, 0x00,
      // ID: SimpleBlock, Payload Size: 10
      0xa3, 0x8a, 0x81, 0x00, 0x00, 0x80,
        // Signal Byte: Clear
        0x00,
        // Frame Data:
        0xde, 0xad, 0xbe, 0xef, 0x00,
      // ID: SimpleBlock, Payload Size: 10
      0xa3, 0x8a, 0x81, 0x03, 0xe8, 0x80,
        // Signal Byte: Clear
        0x00,
        // Frame Data:
        0xde, 0xad, 0xbe, 0xef, 0x00,
      // ID: BlockGroup, Payload Size: 16
      0xa0, 0x90,
        // ID: Block, Payload Size: 10
        0xa1, 0x8a, 0x81, 0x07, 0xd0, 0x00,
          // Signal Byte: Clear
          0x00,
          // Frame Data:
          0xde, 0xad, 0xbe, 0xef, 0x00,
        // BlockDuration: 1000
        0x9b, 0x82, 0x03, 0xe8,
    // ID: Cluster, Payload Size: 50
    0x1f, 0x43, 0xb6, 0x75, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32,
      // Timecode: 3000
      0xe7, 0x82, 0x0b, 0xb8,
      // ID: SimpleBlock: Payload Size: 18
      0xa3, 0x92, 0x81, 0x00,0x00, 0x80,
        // Signal Byte: Encrypted
        0x01,
        // IV:
        0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45,
        // Frame Data:
        0xde, 0xad, 0xbe, 0xef, 0x00,
      // ID: BlockGroup, Payload Size: 24
      0xa0, 0x98,
        // ID: Block, Payload Size: 18
        0xa1, 0x92, 0x81, 0x03, 0xe8, 0x00,
          // Signal Byte: Encrypted
          0x01,
          // IV:
          0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45,
          // Frame Data:
          0xde, 0xad, 0xbe, 0xef, 0x00,
        // BlockDuration: 1000
        0x9b, 0x82, 0x03, 0xe8,
};

}  // namespace

class EncryptedSegmenterTest : public SegmentTestBase {
 public:
  EncryptedSegmenterTest() : info_(CreateVideoStreamInfo(kTimeScale)) {
    EncryptionConfig encryption_config;
    encryption_config.per_sample_iv_size = kPerSampleIvSize;
    encryption_config.key_id.assign(kKeyId, kKeyId + sizeof(kKeyId));
    info_->set_is_encrypted(true);
    info_->set_encryption_config(encryption_config);
  }

 protected:
  void InitializeSegmenter(const MuxerOptions& options) {
    ASSERT_NO_FATAL_FAILURE(
        CreateAndInitializeSegmenter<webm::TwoPassSingleSegmentSegmenter>(
            options, *info_, &segmenter_));
  }

  std::shared_ptr<StreamInfo> info_;
  std::unique_ptr<webm::Segmenter> segmenter_;
};

TEST_F(EncryptedSegmenterTest, BasicSupport) {
  MuxerOptions options = CreateMuxerOptions();
  ASSERT_NO_FATAL_FAILURE(InitializeSegmenter(options));

  // Write the samples to the Segmenter.
  // There should be 2 segments with the first segment in clear and the second
  // segment encrypted.
  for (int i = 0; i < 5; i++) {
    if (i == 3) {
      ASSERT_OK(segmenter_->FinalizeSegment(0, 3 * kDuration, !kSubsegment));
    }
    std::shared_ptr<MediaSample> sample =
        CreateSample(kKeyFrame, kDuration, kNoSideData);
    if (i >= 3) {
      sample->set_is_encrypted(true);
      std::unique_ptr<DecryptConfig> decrypt_config(
          new DecryptConfig(info_->encryption_config().key_id,
                            std::vector<uint8_t>(kIv, kIv + sizeof(kIv)),
                            std::vector<SubsampleEntry>()));
      sample->set_decrypt_config(std::move(decrypt_config));
    }
    ASSERT_OK(segmenter_->AddSample(*sample));
  }
  ASSERT_OK(
      segmenter_->FinalizeSegment(3 * kDuration, 2 * kDuration, !kSubsegment));
  ASSERT_OK(segmenter_->Finalize());

  ASSERT_FILE_ENDS_WITH(OutputFileName().c_str(), kBasicSupportData);
}

}  // namespace media
}  // namespace shaka
