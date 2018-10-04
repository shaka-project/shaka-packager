// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/crypto/sample_aes_ec3_cryptor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/mock_aes_cryptor.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace shaka {
namespace media {

class SampleAesEc3CryptorTest : public ::testing::Test {
 public:
  SampleAesEc3CryptorTest()
      : mock_cryptor_(new MockAesCryptor),
        ec3_cryptor_(std::unique_ptr<MockAesCryptor>(mock_cryptor_)) {}

  void SetUp() {
    std::vector<uint8_t> key(16, 'k');
    std::vector<uint8_t> iv(8, 'i');
    EXPECT_CALL(*mock_cryptor_, InitializeWithIv(key, iv))
        .WillOnce(Return(true));
    EXPECT_TRUE(ec3_cryptor_.InitializeWithIv(key, iv));
    EXPECT_EQ(iv, ec3_cryptor_.iv());
  }

 protected:
  MockAesCryptor* mock_cryptor_;  // Owned by |ec3_cryptor_|.
  SampleAesEc3Cryptor ec3_cryptor_;
};

TEST_F(SampleAesEc3CryptorTest, Crypt) {
  const std::vector<uint8_t> text = {
      // First syncframe with 20 bytes.
      0x0B, 0x77, 0x00, 0x09, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12,
      0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20,
      // Second syncframe with 26 bytes.
      0x0B, 0x77, 0x00, 0x0C, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22,
      0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31, 0x32, 0x33, 0x34,
      0x35, 0x36,
      // Third syncframe with 16 bytes.
      0x0B, 0x77, 0x00, 0x07, 0x15, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31, 0x32,
      0x33, 0x34, 0x35, 0x36};

  EXPECT_CALL(*mock_cryptor_, CryptInternal(_, _, _, _))
      .WillRepeatedly(Invoke([](const uint8_t* text, size_t text_size,
                                uint8_t* crypt_text, size_t* crypt_text_size) {
        *crypt_text_size = text_size;
        for (size_t i = 0; i < text_size; ++i) {
          *crypt_text++ = *text++ + 0x40;
        }
        return true;
      }));

  const std::vector<uint8_t> expected_crypt_text = {
      // First syncframe with 20 bytes.
      0x0B, 0x77, 0x00, 0x09, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12,
      0x13, 0x14, 0x15, 0x16, 0x57, 0x58, 0x59, 0x60,
      // Second syncframe with 26 bytes.
      0x0B, 0x77, 0x00, 0x0C, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22,
      0x23, 0x24, 0x25, 0x26, 0x67, 0x68, 0x69, 0x70, 0x71, 0x72, 0x73, 0x74,
      0x75, 0x76,
      // Third syncframe with 16 bytes.
      0x0B, 0x77, 0x00, 0x07, 0x15, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31, 0x32,
      0x33, 0x34, 0x35, 0x36};

  std::vector<uint8_t> crypt_text;
  ASSERT_TRUE(ec3_cryptor_.Crypt(text, &crypt_text));
  EXPECT_EQ(expected_crypt_text, crypt_text);
}

TEST_F(SampleAesEc3CryptorTest, InvalidEc3Syncword) {
  const std::vector<uint8_t> text = {0x0C, 0x77, 0x00, 0x09, 0x05, 0x06, 0x07,
                                     0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14,
                                     0x15, 0x16, 0x17, 0x18, 0x19, 0x20};

  EXPECT_CALL(*mock_cryptor_, CryptInternal(_, _, _, _))
      .WillRepeatedly(Return(true));

  std::vector<uint8_t> crypt_text;
  ASSERT_FALSE(ec3_cryptor_.Crypt(text, &crypt_text));
}

TEST_F(SampleAesEc3CryptorTest, InvalidEc3SyncframeSize) {
  const std::vector<uint8_t> text = {0x0B, 0x77, 0x00, 0x0A, 0x05, 0x06, 0x07,
                                     0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14,
                                     0x15, 0x16, 0x17, 0x18, 0x19, 0x20};

  EXPECT_CALL(*mock_cryptor_, CryptInternal(_, _, _, _))
      .WillRepeatedly(Return(true));

  std::vector<uint8_t> crypt_text;
  ASSERT_FALSE(ec3_cryptor_.Crypt(text, &crypt_text));
}

}  // namespace media
}  // namespace shaka
