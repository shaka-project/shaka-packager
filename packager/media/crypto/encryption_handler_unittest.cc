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
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/base/test/status_test_util.h"
#include "packager/media/codecs/video_slice_header_parser.h"
#include "packager/media/codecs/vpx_parser.h"

namespace shaka {
namespace media {
namespace {

using ::testing::_;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Values;
using ::testing::WithParamInterface;

class MockKeySource : public FixedKeySource {
 public:
  MOCK_METHOD2(GetKey, Status(TrackType track_type, EncryptionKey* key));
  MOCK_METHOD3(GetCryptoPeriodKey,
               Status(uint32_t crypto_period_index,
                      TrackType track_type,
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

class EncryptionHandlerTest : public MediaHandlerTestBase {
 public:
  void SetUp() override { SetUpEncryptionHandler(EncryptionOptions()); }

  void SetUpEncryptionHandler(const EncryptionOptions& encryption_options) {
    encryption_handler_.reset(
        new EncryptionHandler(encryption_options, &mock_key_source_));
    SetUpGraph(1 /* one input */, 1 /* one output */, encryption_handler_);
  }

  Status Process(std::unique_ptr<StreamData> stream_data) {
    return encryption_handler_->Process(std::move(stream_data));
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
  MockKeySource mock_key_source_;
};

TEST_F(EncryptionHandlerTest, Initialize) {
  ASSERT_OK(encryption_handler_->Initialize());
}

TEST_F(EncryptionHandlerTest, OnlyOneOutput) {
  // Connecting another handler will fail.
  ASSERT_EQ(error::INVALID_ARGUMENT,
            encryption_handler_->AddHandler(some_handler()).error_code());
}

TEST_F(EncryptionHandlerTest, OnlyOneInput) {
  ASSERT_OK(some_handler()->AddHandler(encryption_handler_));
  ASSERT_EQ(error::INVALID_ARGUMENT,
            encryption_handler_->Initialize().error_code());
}

namespace {

const int kStreamIndex = 0;
const bool kEncrypted = true;
const uint32_t kTimeScale = 1000;
const uint32_t kMaxSdPixels = 100u;
const uint32_t kMaxHdPixels = 200u;
const uint32_t kMaxUhd1Pixels = 300u;

// The data is based on H264. The same data is also used to test audio, which
// does not care the underlying data, and VP9, for which we will mock the
// parser.
const uint8_t kData[]{
    // First NALU
    0x15, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    // Second NALU
    0x13, 0x25, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    // Third NALU
    0x06, 0x67, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
};
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

}  // namespace

inline bool operator==(const SubsampleEntry& lhs, const SubsampleEntry& rhs) {
  return lhs.clear_bytes == rhs.clear_bytes &&
         lhs.cipher_bytes == rhs.cipher_bytes;
}

class EncryptionHandlerEncryptionTest
    : public EncryptionHandlerTest,
      public WithParamInterface<std::tr1::tuple<FourCC, Codec>> {
 public:
  void SetUp() override {
    protection_scheme_ = std::tr1::get<0>(GetParam());
    codec_ = std::tr1::get<1>(GetParam());

    EncryptionOptions encryption_options;
    encryption_options.protection_scheme = protection_scheme_;;
    encryption_options.max_sd_pixels = kMaxSdPixels;
    encryption_options.max_hd_pixels = kMaxHdPixels;
    encryption_options.max_uhd1_pixels = kMaxUhd1Pixels;
    SetUpEncryptionHandler(encryption_options);
  }

  std::vector<VPxFrameInfo> GetMockVpxFrameInfo() {
    std::vector<VPxFrameInfo> vpx_frames;
    vpx_frames.resize(2);
    vpx_frames[0].frame_size = 22;
    vpx_frames[0].uncompressed_header_size = 3;
    vpx_frames[1].frame_size = 20;
    vpx_frames[1].uncompressed_header_size = 4;
    return vpx_frames;
  }

  // The subsamples values should match |GetMockVpxFrameInfo| above.
  std::vector<SubsampleEntry> GetExpectedSubsamples() {
    std::vector<SubsampleEntry> subsamples;
    if (codec_ == kCodecAAC)
      return subsamples;
    if (codec_ == kCodecVP9 || protection_scheme_ == FOURCC_cbc1 ||
        protection_scheme_ == FOURCC_cens) {
      // Align the encrypted bytes to multiple of 16 bytes.
      subsamples.emplace_back(6, 16);
    } else {
      subsamples.emplace_back(3, 19);
    }
    subsamples.emplace_back(4, 16);
    subsamples.emplace_back(7, 0);
    return subsamples;
  }

  EncryptionKey GetMockEncryptionKey() {
    EncryptionKey encryption_key;
    encryption_key.key_id.assign(kKeyId, kKeyId + sizeof(kKeyId));
    encryption_key.key.assign(kKey, kKey + sizeof(kKey));
    encryption_key.iv.assign(kIv, kIv + sizeof(kIv));
    return encryption_key;
  }

  bool Decrypt(const DecryptConfig& decrypt_config,
               uint8_t* data,
               size_t data_size) {
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
      if (!aes_decryptor->Crypt(data, data_size, data)) {
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
    switch (protection_scheme_) {
      case FOURCC_cenc:
      case FOURCC_cbc1:
        return 0;
      case FOURCC_cens:
      case FOURCC_cbcs:
        return 1;
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
        return 9;
      default:
        return 0;
    }
  }

 protected:
  FourCC protection_scheme_;
  Codec codec_;
};

TEST_P(EncryptionHandlerEncryptionTest, Encrypt) {
  ASSERT_OK(Process(GetStreamInfoStreamData(kStreamIndex, codec_, kTimeScale)));
  EXPECT_THAT(GetOutputStreamDataVector(),
              ElementsAre(IsStreamInfo(kStreamIndex, kTimeScale, kEncrypted)));

  // Inject vpx parser / video slice header parser if needed.
  switch (codec_) {
    case kCodecVP9:{
      std::unique_ptr<MockVpxParser> mock_vpx_parser(new MockVpxParser);
      EXPECT_CALL(*mock_vpx_parser, Parse(_, sizeof(kData), _))
          .WillOnce(
              DoAll(SetArgPointee<2>(GetMockVpxFrameInfo()), Return(true)));
      InjectVpxParserForTesting(std::move(mock_vpx_parser));
      break;
    }
    case kCodecH264: {
      std::unique_ptr<MockVideoSliceHeaderParser> mock_header_parser(
          new MockVideoSliceHeaderParser);
      // We want to return the same subsamples for VP9 and H264, so the return
      // values here should match |GetMockVpxFrameInfo|.
      EXPECT_CALL(*mock_header_parser, GetHeaderSize(_))
          .WillOnce(Return(1))
          .WillOnce(Return(2));
      InjectVideoSliceHeaderParserForTesting(std::move(mock_header_parser));
      break;
    }
    default:
      break;
  }

  std::unique_ptr<StreamData> stream_data(new StreamData);
  stream_data->stream_index = 0;
  stream_data->stream_data_type = StreamDataType::kMediaSample;
  stream_data->media_sample.reset(
      new MediaSample(kData, sizeof(kData), nullptr, 0, true));

  EXPECT_CALL(mock_key_source_, GetKey(_, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(GetMockEncryptionKey()), Return(Status::OK)));
  ASSERT_OK(Process(std::move(stream_data)));
  ASSERT_EQ(2u, GetOutputStreamDataVector().size());
  ASSERT_EQ(0, GetOutputStreamDataVector().back()->stream_index);
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

  ASSERT_TRUE(Decrypt(*decrypt_config, media_sample->writable_data(),
                      media_sample->data_size()));
  EXPECT_EQ(
      std::vector<uint8_t>(kData, kData + sizeof(kData)),
      std::vector<uint8_t>(media_sample->data(),
                           media_sample->data() + media_sample->data_size()));
}

INSTANTIATE_TEST_CASE_P(
    InstantiationName,
    EncryptionHandlerEncryptionTest,
    Combine(Values(FOURCC_cenc, FOURCC_cens, FOURCC_cbc1, FOURCC_cbcs),
            Values(kCodecAAC, kCodecH264, kCodecVP9)));

// TODO(kqyang): Add more unit tests.

}  // namespace media
}  // namespace shaka
