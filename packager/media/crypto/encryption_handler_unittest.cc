// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/crypto/encryption_handler.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/aes_decryptor.h"
#include "packager/media/base/aes_pattern_cryptor.h"
#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/base/raw_key_source.h"
#include "packager/media/codecs/video_slice_header_parser.h"
#include "packager/media/codecs/vpx_parser.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {

using ::testing::_;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

const char kAudioStreamLabel[] = "AUDIO";
const char kSdVideoStreamLabel[] = "SD";

const uint8_t kKeyId[]{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
};
const uint8_t kKey[]{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
};
const uint8_t kIv[]{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
};

// The default KID for key rotation is all 0s.
const uint8_t kKeyRotationDefaultKeyId[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

class MockKeySource : public RawKeySource {
 public:
  MOCK_METHOD2(GetKey,
               Status(const std::string& stream_label, EncryptionKey* key));
  MOCK_METHOD3(GetCryptoPeriodKey,
               Status(uint32_t crypto_period_index,
                      const std::string& stream_label,
                      EncryptionKey* key));
};

class MockVpxParser : public VPxParser {
 public:
  MOCK_METHOD3(Parse,
               bool(const uint8_t* data,
                    size_t data_size,
                    std::vector<VPxFrameInfo>* vpx_frames));
};

class MockVideoSliceHeaderParser : public VideoSliceHeaderParser {
 public:
  MOCK_METHOD1(Initialize,
               bool(const std::vector<uint8_t>& decoder_configuration));
  MOCK_METHOD1(GetHeaderSize, int64_t(const Nalu& nalu));
};

}  // namespace

class EncryptionHandlerTest : public MediaHandlerGraphTestBase {
 public:
  void SetUp() override { SetUpEncryptionHandler(EncryptionParams()); }

  void SetUpEncryptionHandler(const EncryptionParams& encryption_params) {
    EncryptionParams new_encryption_params = encryption_params;
    if (!encryption_params.stream_label_func) {
      // Setup default stream label function.
      new_encryption_params.stream_label_func =
          [](const EncryptionParams::EncryptedStreamAttributes&
                 stream_attributes) {
            if (stream_attributes.stream_type ==
                EncryptionParams::EncryptedStreamAttributes::kAudio) {
              return kAudioStreamLabel;
            }
            return kSdVideoStreamLabel;
          };
    }
    encryption_handler_.reset(
        new EncryptionHandler(new_encryption_params, &mock_key_source_));
    SetUpGraph(1 /* one input */, 1 /* one output */, encryption_handler_);
  }

  Status Process(std::unique_ptr<StreamData> stream_data) {
    return encryption_handler_->Process(std::move(stream_data));
  }

  EncryptionKey GetMockEncryptionKey() {
    EncryptionKey encryption_key;
    encryption_key.key_id.assign(kKeyId, kKeyId + sizeof(kKeyId));
    encryption_key.key.assign(kKey, kKey + sizeof(kKey));
    encryption_key.iv.assign(kIv, kIv + sizeof(kIv));
    return encryption_key;
  }

  void InjectVpxParserForTesting(std::unique_ptr<VPxParser> vpx_parser) {
    encryption_handler_->InjectVpxParserForTesting(std::move(vpx_parser));
  }

  void InjectVideoSliceHeaderParserForTesting(
      std::unique_ptr<VideoSliceHeaderParser> header_parser) {
    encryption_handler_->InjectVideoSliceHeaderParserForTesting(
        std::move(header_parser));
  }

 protected:
  std::shared_ptr<EncryptionHandler> encryption_handler_;
  StrictMock<MockKeySource> mock_key_source_;
};

TEST_F(EncryptionHandlerTest, Initialize) {
  ASSERT_OK(encryption_handler_->Initialize());
}

TEST_F(EncryptionHandlerTest, OnlyOneOutput) {
  // Connecting another handler will fail.
  ASSERT_OK(encryption_handler_->AddHandler(some_handler()));
  ASSERT_EQ(error::INVALID_ARGUMENT,
            encryption_handler_->Initialize().error_code());
}

TEST_F(EncryptionHandlerTest, OnlyOneInput) {
  ASSERT_OK(some_handler()->AddHandler(encryption_handler_));
  ASSERT_EQ(error::INVALID_ARGUMENT,
            encryption_handler_->Initialize().error_code());
}

namespace {

const bool kVp9SubsampleEncryption = true;
const bool kIsKeyFrame = true;
const bool kIsSubsegment = true;
const bool kEncrypted = true;
const size_t kStreamIndex = 0;
const uint32_t kTimeScale = 1000;
const int64_t kSampleDuration = 1000;
const int64_t kSegmentDuration = 1000;

// The data is based on H264. The same data is also used to test audio, which
// does not care the underlying data, and VP9, for which we will mock the
// parser.
const uint8_t kData[]{
    // First NALU
    0x30, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
    0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    0x46,
    // Second NALU
    0x31, 0x25, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21,
    0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    0x46, 0x47,
    // Third non-video-slice NALU for H264 or superframe index for VP9.
    0x06, 0x67, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
};
const size_t kDataSize = sizeof(kData);
// A short data size (less than leading clear bytes) for SampleAes audio
// testing.
const size_t kShortDataSize = 14;

// H264 subsample information for the the above data.
const size_t kNaluLengthSize = 1u;
const size_t kNaluHeaderSize = 1u;
const size_t kSubsampleSize1 = 49u;
const size_t kSliceHeaderSize1 = 1u;
const size_t kSubsampleSize2 = 50u;
const size_t kSliceHeaderSize2 = 16u;
const size_t kSubsampleSize3 = 7u;
// VP9 frame information for the above data. It should match H264 subsample
// information.
const size_t kVpxFrameSize1 = kSubsampleSize1;
const size_t kUncompressedHeaderSize1 =
    kNaluLengthSize + kNaluHeaderSize + kSliceHeaderSize1;
const size_t kVpxFrameSize2 = kSubsampleSize2;
const size_t kUncompressedHeaderSize2 =
    kNaluLengthSize + kNaluHeaderSize + kSliceHeaderSize2;
// Subsample pairs for the above data.
const size_t kClearSize1 = kUncompressedHeaderSize1;
const size_t kCipherSize1 = kVpxFrameSize1 - kUncompressedHeaderSize1;
const size_t kClearSize2 = kUncompressedHeaderSize2;
const size_t kCipherSize2 = kVpxFrameSize2 - kUncompressedHeaderSize2;
// Align cipher bytes for some protection schemes.
const size_t kAesBlockSize = 16u;
const size_t kAlignedClearSize1 = kClearSize1 + kCipherSize1 % kAesBlockSize;
static_assert(kAlignedClearSize1 != kClearSize1,
              "Clearsize 1 should not be aligned");
const size_t kAlignedCipherSize1 = kCipherSize1 - kCipherSize1 % kAesBlockSize;
// Apple Sample AES.
const size_t kVideoLeadingClearBytesSize = 32u + kNaluLengthSize;
// Subsample 1 is <= 48 bytes, so not encrypted and merged with subsample2.
const size_t kSampleAesClearSize1 =
    kSubsampleSize1 + kVideoLeadingClearBytesSize;
const size_t kSampleAesCipherSize1 =
    kSubsampleSize2 - kVideoLeadingClearBytesSize;

}  // namespace

inline bool operator==(const SubsampleEntry& lhs, const SubsampleEntry& rhs) {
  return lhs.clear_bytes == rhs.clear_bytes &&
         lhs.cipher_bytes == rhs.cipher_bytes;
}

class EncryptionHandlerEncryptionTest
    : public EncryptionHandlerTest,
      public WithParamInterface<std::tr1::tuple<FourCC, Codec, bool>> {
 public:
  void SetUp() override {
    protection_scheme_ = std::tr1::get<0>(GetParam());
    codec_ = std::tr1::get<1>(GetParam());
    vp9_subsample_encryption_ = std::tr1::get<2>(GetParam());
  }

  std::vector<VPxFrameInfo> GetMockVpxFrameInfo() {
    std::vector<VPxFrameInfo> vpx_frames;
    vpx_frames.resize(2);
    vpx_frames[0].frame_size = kVpxFrameSize1;
    vpx_frames[0].uncompressed_header_size = kUncompressedHeaderSize1;
    vpx_frames[1].frame_size = kVpxFrameSize2;
    vpx_frames[1].uncompressed_header_size = kUncompressedHeaderSize2;
    return vpx_frames;
  }

  // The subsamples values should match |GetMockVpxFrameInfo| above.
  std::vector<SubsampleEntry> GetExpectedSubsamples() {
    std::vector<SubsampleEntry> subsamples;
    if (codec_ == kCodecAAC ||
        (codec_ == kCodecVP9 && !vp9_subsample_encryption_)) {
      return subsamples;
    }
    if (protection_scheme_ == kAppleSampleAesProtectionScheme) {
      subsamples.emplace_back(static_cast<uint16_t>(kSampleAesClearSize1),
                              static_cast<uint32_t>(kSampleAesCipherSize1));
      subsamples.emplace_back(static_cast<uint16_t>(kSubsampleSize3), 0u);
    } else {
      if (codec_ == kCodecVP9 || protection_scheme_ == FOURCC_cbc1 ||
          protection_scheme_ == FOURCC_cens ||
          protection_scheme_ == FOURCC_cenc) {
        // Align the encrypted bytes to multiple of 16 bytes.
        subsamples.emplace_back(static_cast<uint16_t>(kAlignedClearSize1),
                                static_cast<uint32_t>(kAlignedCipherSize1));
        // Subsample 2 is already aligned.
      } else {
        subsamples.emplace_back(static_cast<uint16_t>(kClearSize1),
                                static_cast<uint32_t>(kCipherSize1));
      }
      subsamples.emplace_back(static_cast<uint16_t>(kClearSize2),
                              static_cast<uint32_t>(kCipherSize2));
      subsamples.emplace_back(static_cast<uint16_t>(kSubsampleSize3), 0u);
    }
    return subsamples;
  }

  // Inject vpx parser / video slice header parser if needed.
  void InjectCodecParser() {
    switch (codec_) {
      case kCodecVP9:
        if (vp9_subsample_encryption_) {
          std::unique_ptr<MockVpxParser> mock_vpx_parser(new MockVpxParser);
          EXPECT_CALL(*mock_vpx_parser, Parse(_, kDataSize, _))
              .WillRepeatedly(
                  DoAll(SetArgPointee<2>(GetMockVpxFrameInfo()), Return(true)));
          InjectVpxParserForTesting(std::move(mock_vpx_parser));
        }
        break;
      case kCodecH264: {
        std::unique_ptr<MockVideoSliceHeaderParser> mock_header_parser(
            new MockVideoSliceHeaderParser);
        if (protection_scheme_ == kAppleSampleAesProtectionScheme) {
          EXPECT_CALL(*mock_header_parser, GetHeaderSize(_)).Times(0);
        } else {
          EXPECT_CALL(*mock_header_parser, GetHeaderSize(_))
              .WillOnce(Return(kSliceHeaderSize1))
              .WillOnce(Return(kSliceHeaderSize2))
              .WillRepeatedly(Return(kSliceHeaderSize2));
        }
        InjectVideoSliceHeaderParserForTesting(std::move(mock_header_parser));
        break;
      }
      default:
        break;
    }
  }

  bool Decrypt(const DecryptConfig& decrypt_config,
               uint8_t* data,
               size_t data_size) {
    size_t leading_clear_bytes_size = 0;
    std::unique_ptr<AesCryptor> aes_decryptor;
    switch (decrypt_config.protection_scheme()) {
      case FOURCC_cenc:
        aes_decryptor.reset(new AesCtrDecryptor);
        break;
      case FOURCC_cbc1:
        aes_decryptor.reset(new AesCbcDecryptor(kNoPadding));
        break;
      case FOURCC_cens:
        aes_decryptor.reset(new AesPatternCryptor(
            decrypt_config.crypt_byte_block(), decrypt_config.skip_byte_block(),
            AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
            AesCryptor::kDontUseConstantIv,
            std::unique_ptr<AesCryptor>(new AesCtrDecryptor())));
        break;
      case FOURCC_cbcs:
        aes_decryptor.reset(new AesPatternCryptor(
            decrypt_config.crypt_byte_block(), decrypt_config.skip_byte_block(),
            AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
            AesCryptor::kUseConstantIv,
            std::unique_ptr<AesCryptor>(new AesCbcDecryptor(kNoPadding))));
        break;
      case kAppleSampleAesProtectionScheme:
        if (decrypt_config.crypt_byte_block() == 0 &&
            decrypt_config.skip_byte_block() == 0) {
          const size_t kAudioLeadingClearBytesSize = 16u;
          // Only needed for audio; for video, it is already taken into
          // consideration in subsamples.
          leading_clear_bytes_size = kAudioLeadingClearBytesSize;
          aes_decryptor.reset(
              new AesCbcDecryptor(kNoPadding, AesCryptor::kUseConstantIv));
        } else {
          aes_decryptor.reset(new AesPatternCryptor(
              decrypt_config.crypt_byte_block(),
              decrypt_config.skip_byte_block(),
              AesPatternCryptor::kSkipIfCryptByteBlockRemaining,
              AesCryptor::kUseConstantIv,
              std::unique_ptr<AesCryptor>(new AesCbcDecryptor(kNoPadding))));
        }
        break;
      default:
        LOG(FATAL) << "Not supposed to happen.";
    }

    if (!aes_decryptor->InitializeWithIv(
            std::vector<uint8_t>(kKey, kKey + sizeof(kKey)),
            decrypt_config.iv())) {
      return false;
    }

    if (decrypt_config.subsamples().empty()) {
      // Sample not encrypted using subsample encryption. Decrypt whole.
      if (!aes_decryptor->Crypt(data + leading_clear_bytes_size,
                                data_size - leading_clear_bytes_size,
                                data + leading_clear_bytes_size)) {
        LOG(ERROR) << "Error during bulk sample decryption.";
        return false;
      }
      return true;
    }

    // Subsample decryption.
    const std::vector<SubsampleEntry>& subsamples = decrypt_config.subsamples();
    uint8_t* current_ptr = data;
    const uint8_t* const buffer_end = data + data_size;
    for (const auto& subsample : subsamples) {
      if (current_ptr + subsample.clear_bytes + subsample.cipher_bytes >
          buffer_end) {
        LOG(ERROR) << "Subsamples overflow sample buffer.";
        return false;
      }
      current_ptr += subsample.clear_bytes;
      if (!aes_decryptor->Crypt(current_ptr, subsample.cipher_bytes,
                                current_ptr)) {
        LOG(ERROR) << "Error decrypting subsample buffer.";
        return false;
      }
      current_ptr += subsample.cipher_bytes;
    }
    return true;
  }

  uint8_t GetExpectedCryptByteBlock() {
    if (protection_scheme_ == kAppleSampleAesProtectionScheme) {
      // Audio is whole sample encrypted. We could not use a
      // crypto_byte_block_ of 1 for audio as if there is one crypto block
      // remaining, it need not be encrypted for video but it needs to be
      // encrypted for audio.
      return codec_ == kCodecAAC ? 0 : 1;
    }
    switch (protection_scheme_) {
      case FOURCC_cenc:
      case FOURCC_cbc1:
        return 0;
      case FOURCC_cens:
      case FOURCC_cbcs:
        return codec_ == kCodecAAC ? 0 : 1;
      default:
        return 0;
    }
  }

  uint8_t GetExpectedSkipByteBlock() {
    // Always use full sample encryption for audio.
    if (codec_ == kCodecAAC)
      return 0;
    switch (protection_scheme_) {
      case FOURCC_cenc:
      case FOURCC_cbc1:
        return 0;
      case FOURCC_cens:
      case FOURCC_cbcs:
      case kAppleSampleAesProtectionScheme:
        return 9;
      default:
        return 0;
    }
  }

  uint8_t GetExpectedPerSampleIvSize() {
    switch (protection_scheme_) {
      case FOURCC_cenc:
      case FOURCC_cens:
      case FOURCC_cbc1:
        return sizeof(kIv);
      case FOURCC_cbcs:
      case kAppleSampleAesProtectionScheme:
        return 0;
      default:
        return 0;
    }
  }

  std::vector<uint8_t> GetExpectedConstantIv() {
    switch (protection_scheme_) {
      case FOURCC_cbcs:
      case kAppleSampleAesProtectionScheme:
        return std::vector<uint8_t>(std::begin(kIv), std::end(kIv));
      default:
        return std::vector<uint8_t>();
    }
  }

 protected:
  FourCC protection_scheme_;
  Codec codec_;
  bool vp9_subsample_encryption_;
};

TEST_P(EncryptionHandlerEncryptionTest, ClearLeadWithNoKeyRotation) {
  const double kClearLeadInSeconds = 1.5 * kSegmentDuration / kTimeScale;
  EncryptionParams encryption_params;
  encryption_params.protection_scheme = protection_scheme_;
  encryption_params.clear_lead_in_seconds = kClearLeadInSeconds;
  encryption_params.vp9_subsample_encryption = vp9_subsample_encryption_;
  SetUpEncryptionHandler(encryption_params);

  const EncryptionKey mock_encryption_key = GetMockEncryptionKey();
  EXPECT_CALL(mock_key_source_, GetKey(_, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mock_encryption_key), Return(Status::OK)));

  if (IsVideoCodec(codec_)) {
    ASSERT_OK(Process(StreamData::FromStreamInfo(
        kStreamIndex, GetVideoStreamInfo(kTimeScale, codec_))));
  } else {
    ASSERT_OK(Process(StreamData::FromStreamInfo(
        kStreamIndex, GetAudioStreamInfo(kTimeScale, codec_))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex, kTimeScale, kEncrypted, _)));
  const StreamInfo* stream_info =
      GetOutputStreamDataVector().back()->stream_info.get();
  ASSERT_TRUE(stream_info);
  EXPECT_TRUE(stream_info->has_clear_lead());
  EXPECT_THAT(stream_info->encryption_config(),
              MatchEncryptionConfig(
                  protection_scheme_, GetExpectedCryptByteBlock(),
                  GetExpectedSkipByteBlock(), GetExpectedPerSampleIvSize(),
                  GetExpectedConstantIv(), mock_encryption_key.key_id));
  ClearOutputStreamDataVector();
  Mock::VerifyAndClearExpectations(&mock_key_source_);

  InjectCodecParser();

  // There are three segments. Only the third segment is encrypted.
  for (int i = 0; i < 3; ++i) {
    // Use single-frame segment for testing.
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex, GetMediaSample(i * kSegmentDuration, kSegmentDuration,
                                     kIsKeyFrame, kData, kDataSize))));
    ASSERT_OK(Process(StreamData::FromSegmentInfo(
        kStreamIndex, GetSegmentInfo(i * kSegmentDuration, kSegmentDuration,
                                     !kIsSubsegment))));
    const bool is_encrypted = i == 2;
    const auto& output_stream_data = GetOutputStreamDataVector();
    EXPECT_THAT(output_stream_data,
                ElementsAre(IsMediaSample(kStreamIndex, i * kSegmentDuration,
                                          kSegmentDuration, is_encrypted, _),
                            IsSegmentInfo(kStreamIndex, i * kSegmentDuration,
                                          kSegmentDuration, !kIsSubsegment,
                                          is_encrypted)));
    EXPECT_FALSE(output_stream_data.back()
                     ->segment_info->key_rotation_encryption_config);
    ClearOutputStreamDataVector();
  }
}

TEST_P(EncryptionHandlerEncryptionTest, ClearLeadWithKeyRotation) {
  const double kClearLeadInSeconds = 1.5 * kSegmentDuration / kTimeScale;
  const int kSegmentsPerCryptoPeriod = 2;  // 2 segments.
  const double kCryptoPeriodDurationInSeconds =
      kSegmentsPerCryptoPeriod * kSegmentDuration / kTimeScale;
  EncryptionParams encryption_params;
  encryption_params.protection_scheme = protection_scheme_;
  encryption_params.clear_lead_in_seconds = kClearLeadInSeconds;
  encryption_params.crypto_period_duration_in_seconds =
      kCryptoPeriodDurationInSeconds;
  encryption_params.vp9_subsample_encryption = vp9_subsample_encryption_;
  SetUpEncryptionHandler(encryption_params);

  if (IsVideoCodec(codec_)) {
    ASSERT_OK(Process(StreamData::FromStreamInfo(
        kStreamIndex, GetVideoStreamInfo(kTimeScale, codec_))));
  } else {
    ASSERT_OK(Process(StreamData::FromStreamInfo(
        kStreamIndex, GetAudioStreamInfo(kTimeScale, codec_))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex, kTimeScale, kEncrypted, _)));
  const StreamInfo* stream_info =
      GetOutputStreamDataVector().back()->stream_info.get();
  ASSERT_TRUE(stream_info);
  EXPECT_TRUE(stream_info->has_clear_lead());
  const EncryptionConfig& encryption_config = stream_info->encryption_config();
  EXPECT_EQ(protection_scheme_, encryption_config.protection_scheme);
  EXPECT_EQ(GetExpectedCryptByteBlock(), encryption_config.crypt_byte_block);
  EXPECT_EQ(GetExpectedSkipByteBlock(), encryption_config.skip_byte_block);
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kKeyRotationDefaultKeyId),
                                 std::end(kKeyRotationDefaultKeyId)),
            encryption_config.key_id);
  ClearOutputStreamDataVector();

  InjectCodecParser();

  // There are five segments with the first two not encrypted.
  for (int i = 0; i < 5; ++i) {
    if ((i % kSegmentsPerCryptoPeriod) == 0) {
      EXPECT_CALL(mock_key_source_,
                  GetCryptoPeriodKey(i / kSegmentsPerCryptoPeriod, _, _))
          .WillOnce(DoAll(SetArgPointee<2>(GetMockEncryptionKey()),
                          Return(Status::OK)));
    }
    // Use single-frame segment for testing.
    ASSERT_OK(Process(StreamData::FromMediaSample(
        kStreamIndex, GetMediaSample(i * kSegmentDuration, kSegmentDuration,
                                     kIsKeyFrame, kData, kDataSize))));
    ASSERT_OK(Process(StreamData::FromSegmentInfo(
        kStreamIndex, GetSegmentInfo(i * kSegmentDuration, kSegmentDuration,
                                     !kIsSubsegment))));
    const bool is_encrypted = i >= 2;
    const auto& output_stream_data = GetOutputStreamDataVector();
    EXPECT_THAT(output_stream_data,
                ElementsAre(IsMediaSample(kStreamIndex, i * kSegmentDuration,
                                          kSegmentDuration, is_encrypted, _),
                            IsSegmentInfo(kStreamIndex, i * kSegmentDuration,
                                          kSegmentDuration, !kIsSubsegment,
                                          is_encrypted)));
    EXPECT_THAT(*output_stream_data.back()
                     ->segment_info->key_rotation_encryption_config,
                MatchEncryptionConfig(
                    protection_scheme_, GetExpectedCryptByteBlock(),
                    GetExpectedSkipByteBlock(), GetExpectedPerSampleIvSize(),
                    GetExpectedConstantIv(), GetMockEncryptionKey().key_id));
    Mock::VerifyAndClearExpectations(&mock_key_source_);
    ClearOutputStreamDataVector();
  }
}

TEST_P(EncryptionHandlerEncryptionTest, Encrypt) {
  EncryptionParams encryption_params;
  encryption_params.protection_scheme = protection_scheme_;
  encryption_params.vp9_subsample_encryption = vp9_subsample_encryption_;
  SetUpEncryptionHandler(encryption_params);

  const EncryptionKey mock_encryption_key = GetMockEncryptionKey();
  EXPECT_CALL(mock_key_source_, GetKey(_, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mock_encryption_key), Return(Status::OK)));

  if (IsVideoCodec(codec_)) {
    ASSERT_OK(Process(StreamData::FromStreamInfo(
        kStreamIndex, GetVideoStreamInfo(kTimeScale, codec_))));
  } else {
    ASSERT_OK(Process(StreamData::FromStreamInfo(
        kStreamIndex, GetAudioStreamInfo(kTimeScale, codec_))));
  }

  EXPECT_THAT(
      GetOutputStreamDataVector(),
      ElementsAre(IsStreamInfo(kStreamIndex, kTimeScale, kEncrypted, _)));
  const StreamInfo* stream_info =
      GetOutputStreamDataVector().back()->stream_info.get();
  ASSERT_TRUE(stream_info);
  EXPECT_FALSE(stream_info->has_clear_lead());

  InjectCodecParser();

  ASSERT_OK(Process(StreamData::FromMediaSample(
      kStreamIndex,
      GetMediaSample(0, kSampleDuration, kIsKeyFrame, kData, kDataSize))));
  ASSERT_EQ(2u, GetOutputStreamDataVector().size());
  ASSERT_EQ(kStreamIndex, GetOutputStreamDataVector().back()->stream_index);
  ASSERT_EQ(StreamDataType::kMediaSample,
            GetOutputStreamDataVector().back()->stream_data_type);

  auto* media_sample = GetOutputStreamDataVector().back()->media_sample.get();
  auto* decrypt_config = media_sample->decrypt_config();
  EXPECT_EQ(std::vector<uint8_t>(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::vector<uint8_t>(kIv, kIv + sizeof(kIv)), decrypt_config->iv());
  EXPECT_EQ(GetExpectedSubsamples(), decrypt_config->subsamples());
  EXPECT_EQ(protection_scheme_, decrypt_config->protection_scheme());
  EXPECT_EQ(GetExpectedCryptByteBlock(), decrypt_config->crypt_byte_block());
  EXPECT_EQ(GetExpectedSkipByteBlock(), decrypt_config->skip_byte_block());

  std::vector<uint8_t> expected(kData, kData + kDataSize);
  std::vector<uint8_t> actual(media_sample->data(),
                              media_sample->data() + media_sample->data_size());
  ASSERT_TRUE(Decrypt(*decrypt_config, actual.data(), actual.size()));
  EXPECT_EQ(expected, actual);
}

// Verify that the data in short audio (less than leading clear bytes) is left
// unencrypted.
TEST_P(EncryptionHandlerEncryptionTest, SampleAesEncryptShortAudio) {
  if (IsVideoCodec(codec_) ||
      protection_scheme_ != kAppleSampleAesProtectionScheme) {
    return;
  }
  EncryptionParams encryption_params;
  encryption_params.protection_scheme = kAppleSampleAesProtectionScheme;
  SetUpEncryptionHandler(encryption_params);

  const EncryptionKey mock_encryption_key = GetMockEncryptionKey();
  EXPECT_CALL(mock_key_source_, GetKey(_, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mock_encryption_key), Return(Status::OK)));

  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex, GetAudioStreamInfo(kTimeScale, codec_))));

  ASSERT_OK(Process(StreamData::FromMediaSample(
      kStreamIndex,
      GetMediaSample(0, kSampleDuration, kIsKeyFrame, kData, kShortDataSize))));
  ASSERT_EQ(2u, GetOutputStreamDataVector().size());
  ASSERT_EQ(kStreamIndex, GetOutputStreamDataVector().back()->stream_index);
  ASSERT_EQ(StreamDataType::kMediaSample,
            GetOutputStreamDataVector().back()->stream_data_type);

  auto* media_sample = GetOutputStreamDataVector().back()->media_sample.get();
  auto* decrypt_config = media_sample->decrypt_config();
  EXPECT_TRUE(decrypt_config->subsamples().empty());
  EXPECT_EQ(kAppleSampleAesProtectionScheme,
            decrypt_config->protection_scheme());

  std::vector<uint8_t> expected(kData, kData + kShortDataSize);
  std::vector<uint8_t> actual(media_sample->data(),
                              media_sample->data() + media_sample->data_size());
  EXPECT_EQ(expected, actual);
}

INSTANTIATE_TEST_CASE_P(
    CencProtectionSchemes,
    EncryptionHandlerEncryptionTest,
    Combine(Values(FOURCC_cenc, FOURCC_cens, FOURCC_cbc1, FOURCC_cbcs),
            Values(kCodecAAC, kCodecH264, kCodecVP9),
            Values(kVp9SubsampleEncryption, !kVp9SubsampleEncryption)));
INSTANTIATE_TEST_CASE_P(AppleSampleAes,
                        EncryptionHandlerEncryptionTest,
                        Combine(Values(kAppleSampleAesProtectionScheme),
                                Values(kCodecAAC, kCodecH264),
                                Values(kVp9SubsampleEncryption)));

class EncryptionHandlerTrackTypeTest : public EncryptionHandlerTest {
 public:
  void SetUp() override {}
};

TEST_F(EncryptionHandlerTrackTypeTest, AudioTrackType) {
  EncryptionParams::EncryptedStreamAttributes captured_stream_attributes;
  EncryptionParams encryption_params;
  encryption_params.stream_label_func =
      [&captured_stream_attributes](
          const EncryptionParams::EncryptedStreamAttributes&
              stream_attributes) {
        captured_stream_attributes = stream_attributes;
        return kAudioStreamLabel;
      };
  SetUpEncryptionHandler(encryption_params);
  EXPECT_CALL(mock_key_source_, GetKey(kAudioStreamLabel, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(GetMockEncryptionKey()), Return(Status::OK)));
  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex, GetAudioStreamInfo(kTimeScale))));
  EXPECT_EQ(EncryptionParams::EncryptedStreamAttributes::kAudio,
            captured_stream_attributes.stream_type);
}

TEST_F(EncryptionHandlerTrackTypeTest, VideoTrackType) {
  const int32_t kWidth = 12;
  const int32_t kHeight = 34;
  EncryptionParams::EncryptedStreamAttributes captured_stream_attributes;
  EncryptionParams encryption_params;
  encryption_params.stream_label_func =
      [&captured_stream_attributes](
          const EncryptionParams::EncryptedStreamAttributes&
              stream_attributes) {
        captured_stream_attributes = stream_attributes;
        return kSdVideoStreamLabel;
      };
  SetUpEncryptionHandler(encryption_params);
  EXPECT_CALL(mock_key_source_, GetKey(kSdVideoStreamLabel, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(GetMockEncryptionKey()), Return(Status::OK)));
  ASSERT_OK(Process(StreamData::FromStreamInfo(
      kStreamIndex, GetVideoStreamInfo(kTimeScale, kWidth, kHeight))));
  EXPECT_EQ(EncryptionParams::EncryptedStreamAttributes::kVideo,
            captured_stream_attributes.stream_type);
  EXPECT_EQ(captured_stream_attributes.oneof.video.width, kWidth);
  EXPECT_EQ(captured_stream_attributes.oneof.video.height, kHeight);
}

}  // namespace media
}  // namespace shaka
