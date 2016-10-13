// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/aes_pattern_cryptor.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {
const uint8_t kCryptByteBlock = 2u;
const uint8_t kSkipByteBlock = 1u;
}  // namespace

namespace shaka {
namespace media {

class MockAesCryptor : public AesCryptor {
 public:
  MockAesCryptor() : AesCryptor(kDontUseConstantIv) {}

  MOCK_METHOD2(InitializeWithIv,
               bool(const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& iv));
  MOCK_METHOD4(CryptInternal,
               bool(const uint8_t* text,
                    size_t text_size,
                    uint8_t* crypt_text,
                    size_t* crypt_text_size));
  MOCK_METHOD0(SetIvInternal, void());
};

class AesPatternCryptorTest : public ::testing::Test {
 public:
  AesPatternCryptorTest()
      : mock_cryptor_(new MockAesCryptor),
        pattern_cryptor_(kCryptByteBlock,
                         kSkipByteBlock,
                         AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
                         AesCryptor::kDontUseConstantIv,
                         std::unique_ptr<MockAesCryptor>(mock_cryptor_)) {}

 protected:
  MockAesCryptor* mock_cryptor_;
  AesPatternCryptor pattern_cryptor_;
};

TEST_F(AesPatternCryptorTest, InitializeWithIv) {
  std::vector<uint8_t> key(16, 'k');
  std::vector<uint8_t> iv(8, 'i');
  EXPECT_CALL(*mock_cryptor_, InitializeWithIv(key, iv)).WillOnce(Return(true));
  EXPECT_TRUE(pattern_cryptor_.InitializeWithIv(key, iv));
  EXPECT_EQ(iv, pattern_cryptor_.iv());
}

namespace {

struct PatternTestCase {
  const char* text_hex;
  const char* expected_crypt_text_hex;
};

const PatternTestCase kPatternTestCases[] = {
    // Empty.
    {"", ""},
    // One partial block (not encrypted).
    {"010203", "010203"},
    // One block (not encrypted).
    {"01020304050607080910111213141516", "01020304050607080910111213141516"},
    // One block + partial block (not encrypted).
    {"010203040506070809101112131415161718",
     "010203040506070809101112131415161718"},
    // Two blocks (encrypted) - the mock encryptor adds every byte by 0x10.
    {"0102030405060708091011121314151617181920212223242526272829303132",
     "1112131415161718192021222324252627282930313233343536373839404142"},
    // Two blocks + partial block (only the first two blocks are encrypted).
    {"0102030405060708091011121314151617181920212223242526272829303132"
     "333435363738",
     "1112131415161718192021222324252627282930313233343536373839404142"
     "333435363738"},
    // Seven blocks.
    {// kCryptByteBlock (2) blocks encrypted.
     "0102030405060708091011121314151617181920212223242526272829303132"
     // kSkipByteBlock (1) block not encrypted.
     "33343536373839404142434445464748"
     // kCryptByteBlock (2) blocks encrypted.
     "4950515253545556575859606162636465666768697071727374757677787980"
     // kSkipByteBlock (1) block not encrypted.
     "81828384858687888990919293949596"
     // Less than kCryptByteBlock blocks remaining, so not encrypted.
     "97989900010203040506070809101112",
     "1112131415161718192021222324252627282930313233343536373839404142"
     "33343536373839404142434445464748"
     "5960616263646566676869707172737475767778798081828384858687888990"
     "81828384858687888990919293949596"
     "97989900010203040506070809101112"},
};

}  // namespace

class AesPatternCryptorVerificationTest
    : public AesPatternCryptorTest,
      public ::testing::WithParamInterface<PatternTestCase> {};

TEST_P(AesPatternCryptorVerificationTest, PatternTest) {
  std::vector<uint8_t> text;
  std::string text_hex(GetParam().text_hex);
  if (!text_hex.empty())
    ASSERT_TRUE(base::HexStringToBytes(text_hex, &text));
  std::vector<uint8_t> expected_crypt_text;
  std::string expected_crypt_text_hex(GetParam().expected_crypt_text_hex);
  if (!expected_crypt_text_hex.empty()) {
    ASSERT_TRUE(
        base::HexStringToBytes(expected_crypt_text_hex, &expected_crypt_text));
  }

  ON_CALL(*mock_cryptor_, CryptInternal(_, _, _, _))
      .WillByDefault(Invoke([](const uint8_t* text, size_t text_size,
                               uint8_t* crypt_text, size_t* crypt_text_size) {
        *crypt_text_size = text_size;
        for (size_t i = 0; i < text_size; ++i) {
          *crypt_text++ = *text++ + 0x10;
        }
        return true;
      }));

  std::vector<uint8_t> crypt_text;
  ASSERT_TRUE(pattern_cryptor_.Crypt(text, &crypt_text));
  EXPECT_EQ(expected_crypt_text, crypt_text);
}

INSTANTIATE_TEST_CASE_P(PatternTestCases,
                        AesPatternCryptorVerificationTest,
                        ::testing::ValuesIn(kPatternTestCases));

TEST(AesPatternCryptorConstIvTest, UseConstantIv) {
  MockAesCryptor* mock_cryptor = new MockAesCryptor;
  AesPatternCryptor pattern_cryptor(
      kCryptByteBlock, kSkipByteBlock,
      AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
      AesPatternCryptor::kUseConstantIv,
      std::unique_ptr<MockAesCryptor>(mock_cryptor));

  std::vector<uint8_t> iv(8, 'i');
  // SetIv will be called twice:
  //   once by AesPatternCryptor::SetIv,
  //   once by AesPatternCryptor::Crypt, to make sure the same iv is used.
  EXPECT_CALL(*mock_cryptor, SetIvInternal()).Times(2);
  EXPECT_TRUE(pattern_cryptor.SetIv(iv));

  std::string crypt_text;
  ASSERT_TRUE(pattern_cryptor.Crypt("010203", &crypt_text));
}

TEST(AesPatternCryptorConstIvTest, DontUseConstantIv) {
  MockAesCryptor* mock_cryptor = new MockAesCryptor;
  AesPatternCryptor pattern_cryptor(
      kCryptByteBlock, kSkipByteBlock,
      AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
      AesPatternCryptor::kDontUseConstantIv,
      std::unique_ptr<MockAesCryptor>(mock_cryptor));

  std::vector<uint8_t> iv(8, 'i');
  // SetIv will be called only once by AesPatternCryptor::SetIv.
  EXPECT_CALL(*mock_cryptor, SetIvInternal());
  EXPECT_TRUE(pattern_cryptor.SetIv(iv));

  std::string crypt_text;
  ASSERT_TRUE(pattern_cryptor.Crypt("010203", &crypt_text));
}

TEST(SampleAesPatternCryptor, 16Bytes) {
  MockAesCryptor* mock_cryptor = new MockAesCryptor();
  EXPECT_CALL(*mock_cryptor, CryptInternal(_, _, _, _)).Times(0);

  const uint8_t kSampleAesEncryptedBlock = 1;
  const uint8_t kSampleAesClearBlock = 9;
  AesPatternCryptor pattern_cryptor(
      kSampleAesEncryptedBlock, kSampleAesClearBlock,
      AesPatternCryptor::kSkipIfCryptByteBlockRemaining,
      AesPatternCryptor::kUseConstantIv,
      std::unique_ptr<MockAesCryptor>(mock_cryptor));

  std::vector<uint8_t> iv(8, 'i');
  // SetIv will be called only once by AesPatternCryptor::SetIv.
  EXPECT_TRUE(pattern_cryptor.SetIv(iv));

  std::string crypt_text;
  // Exactly 16 bytes, mock's Crypt should not be called.
  ASSERT_TRUE(pattern_cryptor.Crypt("0123456789abcdef", &crypt_text));
}

TEST(SampleAesPatternCryptor, MoreThan16Bytes) {
  MockAesCryptor* mock_cryptor = new MockAesCryptor();
  EXPECT_CALL(*mock_cryptor, CryptInternal(_, 16u, _, _))
      .WillOnce(Return(true));

  const uint8_t kSampleAesEncryptedBlock = 1;
  const uint8_t kSampleAesClearBlock = 9;
  AesPatternCryptor pattern_cryptor(
      kSampleAesEncryptedBlock, kSampleAesClearBlock,
      AesPatternCryptor::kSkipIfCryptByteBlockRemaining,
      AesPatternCryptor::kUseConstantIv,
      std::unique_ptr<MockAesCryptor>(mock_cryptor));

  std::vector<uint8_t> iv(8, 'i');
  // SetIv will be called only once by AesPatternCryptor::SetIv.
  EXPECT_TRUE(pattern_cryptor.SetIv(iv));

  std::string crypt_text;
  // More than 16 bytes so mock's CryptInternal should be called.
  ASSERT_TRUE(pattern_cryptor.Crypt("0123456789abcdef012", &crypt_text));
}

}  // namespace media
}  // namespace shaka
