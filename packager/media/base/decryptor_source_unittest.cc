// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/decryptor_source.h>

#include <iterator>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/media/base/raw_key_source.h>

using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace shaka {
namespace media {
namespace {

const uint8_t kKeyId[] = {
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
};

const uint8_t kMockKey[] = {
  0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
};

const uint8_t kIv[] = {
  0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
};
const uint8_t kBuffer[] = {
    0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x05, 0x06, 0x07, 0x08, 0x09,
};
// Expected decrypted buffer with the above kMockKey and kIv.
const uint8_t kExpectedDecryptedBuffer[] = {
    0xfd, 0xf9, 0x8b, 0xb2, 0x1d, 0xd3, 0x07, 0x72, 0x51, 0xf4, 0xdf,
    0xf9, 0x16, 0x6a, 0x14, 0xcb, 0xde, 0xaa, 0x6a, 0x04, 0x85,
};

const uint8_t kIv2[] = {
    0x14, 0x25, 0x36, 0x47, 0x58, 0x69, 0x7a, 0x8b,
};
const uint8_t kBuffer2[] = {0x05, 0x02};
// Expected decrypted buffer with the above kMockKey and kIv2.
const uint8_t kExpectedDecryptedBuffer2[] = {0x20, 0x62};

class MockKeySource : public RawKeySource {
 public:
  MOCK_METHOD2(GetKey,
               Status(const std::vector<uint8_t>& key_id, EncryptionKey* key));
};

}  // namespace

class DecryptorSourceTest : public ::testing::Test {
 public:
  DecryptorSourceTest()
      : decryptor_source_(&mock_key_source_),
        key_id_(std::vector<uint8_t>(kKeyId, kKeyId + std::size(kKeyId))),
        encrypted_buffer_(kBuffer, kBuffer + std::size(kBuffer)),
        decrypted_buffer_(std::size(kBuffer)) {}

 protected:
  StrictMock<MockKeySource> mock_key_source_;
  DecryptorSource decryptor_source_;
  std::vector<uint8_t> key_id_;
  std::vector<uint8_t> encrypted_buffer_;
  std::vector<uint8_t> decrypted_buffer_;
};

TEST_F(DecryptorSourceTest, FullSampleDecryption) {
  EncryptionKey encryption_key;
  encryption_key.key.assign(kMockKey, kMockKey + std::size(kMockKey));
  EXPECT_CALL(mock_key_source_, GetKey(key_id_, _))
      .WillOnce(DoAll(SetArgPointee<1>(encryption_key), Return(Status::OK)));

  DecryptConfig decrypt_config(key_id_,
                               std::vector<uint8_t>(kIv, kIv + std::size(kIv)),
                               std::vector<SubsampleEntry>());
  ASSERT_TRUE(decryptor_source_.DecryptSampleBuffer(
      &decrypt_config, &encrypted_buffer_[0], encrypted_buffer_.size(),
      &decrypted_buffer_[0]));
  EXPECT_EQ(std::vector<uint8_t>(
                kExpectedDecryptedBuffer,
                kExpectedDecryptedBuffer + std::size(kExpectedDecryptedBuffer)),
            decrypted_buffer_);

  // DecryptSampleBuffer can be called repetitively. No GetKey call again with
  // the same key id.
  encrypted_buffer_.assign(kBuffer2, kBuffer2 + std::size(kBuffer2));
  decrypted_buffer_.resize(std::size(kBuffer2));
  DecryptConfig decrypt_config2(
      key_id_, std::vector<uint8_t>(kIv2, kIv2 + std::size(kIv2)),
      std::vector<SubsampleEntry>());
  ASSERT_TRUE(decryptor_source_.DecryptSampleBuffer(
      &decrypt_config2, &encrypted_buffer_[0], encrypted_buffer_.size(),
      &decrypted_buffer_[0]));
  EXPECT_EQ(std::vector<uint8_t>(kExpectedDecryptedBuffer2,
                                 kExpectedDecryptedBuffer2 +
                                     std::size(kExpectedDecryptedBuffer2)),
            decrypted_buffer_);
}

TEST_F(DecryptorSourceTest, SubsampleDecryption) {
  EncryptionKey encryption_key;
  encryption_key.key.assign(kMockKey, kMockKey + std::size(kMockKey));
  EXPECT_CALL(mock_key_source_, GetKey(key_id_, _))
      .WillOnce(DoAll(SetArgPointee<1>(encryption_key), Return(Status::OK)));

  const SubsampleEntry kSubsamples[] = {
    {2, 3},
    {3, 13},
  };
  // Expected decrypted buffer with the above subsamples.
  // clang-format off
  const uint8_t kExpectedDecryptedSubsampleBuffer[] = {
    // Subsample[0].clear
    0x03, 0x04,
    // Subsample[0].cipher
    0xfb, 0xfb, 0x89,
    // Subsample[1].clear
    0x08, 0x09, 0x0a,
    // Subsample[1].cipher
    0xb0, 0x1f, 0xdd, 0x09, 0x70, 0x5c, 0xfb, 0xd2,
    0xfb, 0x18, 0x64, 0x16, 0xc9,
  };
  // clang-format on

  DecryptConfig decrypt_config(
      key_id_, std::vector<uint8_t>(kIv, kIv + std::size(kIv)),
      std::vector<SubsampleEntry>(kSubsamples,
                                  kSubsamples + std::size(kSubsamples)));
  ASSERT_TRUE(decryptor_source_.DecryptSampleBuffer(
      &decrypt_config, &encrypted_buffer_[0], encrypted_buffer_.size(),
      &decrypted_buffer_[0]));
  EXPECT_EQ(
      std::vector<uint8_t>(kExpectedDecryptedSubsampleBuffer,
                           kExpectedDecryptedSubsampleBuffer +
                               std::size(kExpectedDecryptedSubsampleBuffer)),
      decrypted_buffer_);
}

TEST_F(DecryptorSourceTest, SubsampleDecryptionSizeValidation) {
  EncryptionKey encryption_key;
  encryption_key.key.assign(kMockKey, kMockKey + std::size(kMockKey));
  EXPECT_CALL(mock_key_source_, GetKey(key_id_, _))
      .WillOnce(DoAll(SetArgPointee<1>(encryption_key), Return(Status::OK)));

  // Total size exceeds buffer size.
  const SubsampleEntry kSubsamples[] = {
    {2, 3},
    {3, 14},
  };

  DecryptConfig decrypt_config(
      key_id_, std::vector<uint8_t>(kIv, kIv + std::size(kIv)),
      std::vector<SubsampleEntry>(kSubsamples,
                                  kSubsamples + std::size(kSubsamples)));
  ASSERT_FALSE(decryptor_source_.DecryptSampleBuffer(
      &decrypt_config, &encrypted_buffer_[0], encrypted_buffer_.size(),
      &decrypted_buffer_[0]));
}

TEST_F(DecryptorSourceTest, DecryptFailedIfGetKeyFailed) {
  EXPECT_CALL(mock_key_source_, GetKey(key_id_, _))
      .WillOnce(Return(Status::UNKNOWN));

  DecryptConfig decrypt_config(key_id_,
                               std::vector<uint8_t>(kIv, kIv + std::size(kIv)),
                               std::vector<SubsampleEntry>());
  ASSERT_FALSE(decryptor_source_.DecryptSampleBuffer(
      &decrypt_config, &encrypted_buffer_[0], encrypted_buffer_.size(),
      &decrypted_buffer_[0]));
}

TEST_F(DecryptorSourceTest, EncryptedBufferAndDecryptedBufferOverlap) {
  DecryptConfig decrypt_config(key_id_,
                               std::vector<uint8_t>(kIv, kIv + std::size(kIv)),
                               std::vector<SubsampleEntry>());
  ASSERT_FALSE(decryptor_source_.DecryptSampleBuffer(
      &decrypt_config, &encrypted_buffer_[0], encrypted_buffer_.size(),
      &encrypted_buffer_[5]));
}

}  // namespace media
}  // namespace shaka
