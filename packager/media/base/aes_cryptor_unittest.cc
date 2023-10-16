// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/aes_decryptor.h>
#include <packager/media/base/aes_encryptor.h>

#include <iterator>
#include <memory>

#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <gtest/gtest.h>

#include <packager/utils/bytes_to_string_view.h>

namespace {

const uint32_t kAesBlockSize = 16;

// From NIST SP 800-38a test case: - F.5.1 CTR-AES128.Encrypt
// http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
const uint8_t kAesKey[] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                           0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};

const uint8_t kAesIv[] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                          0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

const uint8_t kAesCtrPlaintext[] = {
    // Block #1
    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
    // Block #2
    0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
    0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
    // Block #3
    0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
    0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
    // Block #4
    0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
    0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10};

const uint8_t kAesCtrCiphertext[] = {
    // Block #1
    0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26,
    0x1b, 0xef, 0x68, 0x64, 0x99, 0x0d, 0xb6, 0xce,
    // Block #2
    0x98, 0x06, 0xf6, 0x6b, 0x79, 0x70, 0xfd, 0xff,
    0x86, 0x17, 0x18, 0x7b, 0xb9, 0xff, 0xfd, 0xff,
    // Block #3
    0x5a, 0xe4, 0xdf, 0x3e, 0xdb, 0xd5, 0xd3, 0x5e,
    0x5b, 0x4f, 0x09, 0x02, 0x0d, 0xb0, 0x3e, 0xab,
    // Block #4
    0x1e, 0x03, 0x1d, 0xda, 0x2f, 0xbe, 0x03, 0xd1,
    0x79, 0x21, 0x70, 0xa0, 0xf3, 0x00, 0x9c, 0xee};

const uint8_t kSubsampleTest1[] = {64};
const uint8_t kSubsampleTest2[] = {13, 51};
const uint8_t kSubsampleTest3[] = {52, 12};
const uint8_t kSubsampleTest4[] = {16, 48};
const uint8_t kSubsampleTest5[] = {3, 16, 45};
const uint8_t kSubsampleTest6[] = {18, 12, 34};
const uint8_t kSubsampleTest7[] = {8, 16, 2, 38};
const uint8_t kSubsampleTest8[] = {10, 1, 33, 20};
const uint8_t kSubsampleTest9[] = {7, 19, 6, 32};
const uint8_t kSubsampleTest10[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9};

// IV test values.
const uint32_t kTextSizeInBytes = 60;  // 3 full blocks + 1 partial block.

const uint8_t kIv128Zero[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const uint8_t kIv128Two[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
const uint8_t kIv128Four[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4};
const uint8_t kIv128Max64[] = {0,    0,    0,    0,    0,    0,    0,    0,
                               0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
const uint8_t kIv128OneAndThree[] = {0, 0, 0, 0, 0, 0, 0, 1,
                                     0, 0, 0, 0, 0, 0, 0, 3};
const uint8_t kIv128MaxMinusOne[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xfe};

const uint8_t kIv64Zero[] = {0, 0, 0, 0, 0, 0, 0, 0};
const uint8_t kIv64One[] = {0, 0, 0, 0, 0, 0, 0, 1};
const uint8_t kIv64MaxMinusOne[] = {0xff, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xfe};
const uint8_t kIv64Max[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// We support AES 128, i.e. 16 bytes key only.
const uint8_t kInvalidKey[] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2,
                               0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09};

// We support Iv of size 8 or 16 only as defined in CENC spec.
const uint8_t kInvalidIv[] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                              0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe};

}  // namespace

namespace shaka {
namespace media {

class AesCtrEncryptorTest : public testing::Test {
 public:
  void SetUp() override {
    key_.assign(kAesKey, kAesKey + std::size(kAesKey));
    iv_.assign(kAesIv, kAesIv + std::size(kAesIv));
    plaintext_.assign(kAesCtrPlaintext,
                      kAesCtrPlaintext + std::size(kAesCtrPlaintext));
    ciphertext_.assign(kAesCtrCiphertext,
                       kAesCtrCiphertext + std::size(kAesCtrCiphertext));

    ASSERT_TRUE(encryptor_.InitializeWithIv(key_, iv_));
    ASSERT_TRUE(decryptor_.InitializeWithIv(key_, iv_));
  }

 protected:
  std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;
  std::vector<uint8_t> plaintext_;
  std::vector<uint8_t> ciphertext_;
  AesCtrEncryptor encryptor_;
  AesCtrDecryptor decryptor_;
};

TEST_F(AesCtrEncryptorTest, NistTestCase) {
  std::vector<uint8_t> encrypted;
  ASSERT_TRUE(encryptor_.Crypt(plaintext_, &encrypted));
  EXPECT_EQ(ciphertext_, encrypted);

  ASSERT_TRUE(decryptor_.SetIv(iv_));
  std::vector<uint8_t> decrypted;
  ASSERT_TRUE(decryptor_.Crypt(encrypted, &decrypted));
  EXPECT_EQ(plaintext_, decrypted);
}

TEST_F(AesCtrEncryptorTest, NistTestCaseInplaceEncryptionDecryption) {
  std::vector<uint8_t> buffer = plaintext_;
  ASSERT_TRUE(encryptor_.Crypt(&buffer[0], buffer.size(), &buffer[0]));
  EXPECT_EQ(ciphertext_, buffer);

  ASSERT_TRUE(decryptor_.SetIv(iv_));
  ASSERT_TRUE(decryptor_.Crypt(&buffer[0], buffer.size(), &buffer[0]));
  EXPECT_EQ(plaintext_, buffer);
}

TEST_F(AesCtrEncryptorTest, EncryptDecryptString) {
  static const char kPlaintext[] = "normal plaintext of random length";
  static const char kExpectedCiphertextInHex[] =
      "82e3ad1ef90c5cc09eb37f1b9efbd99016441a1c15123f0777cd57bb993e14da02";

  std::string ciphertext;
  ASSERT_TRUE(encryptor_.Crypt(kPlaintext, &ciphertext));
  EXPECT_EQ(kExpectedCiphertextInHex, absl::BytesToHexString(ciphertext));

  std::string decrypted;
  ASSERT_TRUE(decryptor_.SetIv(iv_));
  ASSERT_TRUE(decryptor_.Crypt(ciphertext, &decrypted));
  EXPECT_EQ(kPlaintext, decrypted);
}

TEST_F(AesCtrEncryptorTest, 128BitIVBoundaryCaseEncryption) {
  // There are four blocks of text in |plaintext_|. The first block should be
  // encrypted with IV = kIv128Max64, the subsequent blocks should be encrypted
  // with iv 0 to 3.
  std::vector<uint8_t> iv_max64(kIv128Max64,
                                kIv128Max64 + std::size(kIv128Max64));
  ASSERT_TRUE(encryptor_.InitializeWithIv(key_, iv_max64));
  std::vector<uint8_t> encrypted;
  ASSERT_TRUE(encryptor_.Crypt(plaintext_, &encrypted));

  std::vector<uint8_t> iv_one_and_three(
      kIv128OneAndThree, kIv128OneAndThree + std::size(kIv128OneAndThree));
  encryptor_.UpdateIv();
  EXPECT_EQ(iv_one_and_three, encryptor_.iv());

  ASSERT_TRUE(encryptor_.InitializeWithIv(key_, iv_max64));
  std::vector<uint8_t> encrypted_verify(plaintext_.size(), 0);
  ASSERT_TRUE(
      encryptor_.Crypt(&plaintext_[0], kAesBlockSize, &encrypted_verify[0]));
  std::vector<uint8_t> iv_zero(kIv128Zero, kIv128Zero + std::size(kIv128Zero));
  ASSERT_TRUE(encryptor_.InitializeWithIv(key_, iv_zero));
  ASSERT_TRUE(encryptor_.Crypt(&plaintext_[kAesBlockSize], kAesBlockSize * 3,
                               &encrypted_verify[kAesBlockSize]));
  EXPECT_EQ(encrypted, encrypted_verify);
}

TEST_F(AesCtrEncryptorTest, 64BitIvUpdate) {
  std::vector<uint8_t> iv_zero(kIv64Zero, kIv64Zero + std::size(kIv64Zero));
  ASSERT_TRUE(encryptor_.InitializeWithIv(key_, iv_zero));

  // There are four blocks of text in |plaintext_|, but since iv is 8 bytes,
  // iv should only be incremented by one when UpdateIv() is called.
  std::vector<uint8_t> encrypted;
  ASSERT_TRUE(encryptor_.Crypt(plaintext_, &encrypted));

  std::vector<uint8_t> iv_one(kIv64One, kIv64One + std::size(kIv64One));
  encryptor_.UpdateIv();
  EXPECT_EQ(iv_one, encryptor_.iv());
}

TEST_F(AesCtrEncryptorTest, GenerateRandomIv) {
  const uint8_t kCencIvSize = 8;
  std::vector<uint8_t> iv;
  ASSERT_TRUE(AesCryptor::GenerateRandomIv(FOURCC_cenc, &iv));
  ASSERT_EQ(kCencIvSize, iv.size());
  LOG(INFO) << "Random IV: "
            << absl::BytesToHexString(byte_vector_to_string_view(iv));
}

TEST_F(AesCtrEncryptorTest, UnsupportedKeySize) {
  std::vector<uint8_t> key(kInvalidKey, kInvalidKey + std::size(kInvalidKey));
  ASSERT_FALSE(encryptor_.InitializeWithIv(key, iv_));
}

TEST_F(AesCtrEncryptorTest, UnsupportedIV) {
  std::vector<uint8_t> iv(kInvalidIv, kInvalidIv + std::size(kInvalidIv));
  ASSERT_FALSE(encryptor_.InitializeWithIv(key_, iv));
}

// Subsample test cases.
struct SubsampleTestCase {
  const uint8_t* subsample_sizes;
  size_t subsample_count;
};

class AesCtrEncryptorSubsampleTest
    : public AesCtrEncryptorTest,
      public ::testing::WithParamInterface<SubsampleTestCase> {};

TEST_P(AesCtrEncryptorSubsampleTest, NistTestCaseSubsamples) {
  const SubsampleTestCase* test_case = &GetParam();

  std::vector<uint8_t> encrypted(plaintext_.size(), 0);
  for (uint32_t i = 0, offset = 0; i < test_case->subsample_count; ++i) {
    uint32_t len = test_case->subsample_sizes[i];
    ASSERT_TRUE(encryptor_.Crypt(&plaintext_[offset], len, &encrypted[offset]));
    offset += len;
    EXPECT_EQ(offset % kAesBlockSize, encryptor_.block_offset());
  }
  EXPECT_EQ(ciphertext_, encrypted);

  ASSERT_TRUE(decryptor_.SetIv(iv_));
  std::vector<uint8_t> decrypted(encrypted.size(), 0);
  for (uint32_t i = 0, offset = 0; i < test_case->subsample_count; ++i) {
    uint32_t len = test_case->subsample_sizes[i];
    ASSERT_TRUE(decryptor_.Crypt(&encrypted[offset], len, &decrypted[offset]));
    offset += len;
    EXPECT_EQ(offset % kAesBlockSize, decryptor_.block_offset());
  }
  EXPECT_EQ(plaintext_, decrypted);
}

namespace {
const SubsampleTestCase kSubsampleTestCases[] = {
    {kSubsampleTest1, std::size(kSubsampleTest1)},
    {kSubsampleTest2, std::size(kSubsampleTest2)},
    {kSubsampleTest3, std::size(kSubsampleTest3)},
    {kSubsampleTest4, std::size(kSubsampleTest4)},
    {kSubsampleTest5, std::size(kSubsampleTest5)},
    {kSubsampleTest6, std::size(kSubsampleTest6)},
    {kSubsampleTest7, std::size(kSubsampleTest7)},
    {kSubsampleTest8, std::size(kSubsampleTest8)},
    {kSubsampleTest9, std::size(kSubsampleTest9)},
    {kSubsampleTest10, std::size(kSubsampleTest10)}};
}  // namespace

INSTANTIATE_TEST_CASE_P(SubsampleTestCases,
                        AesCtrEncryptorSubsampleTest,
                        ::testing::ValuesIn(kSubsampleTestCases));

struct IvTestCase {
  const uint8_t* iv_test;
  size_t iv_size;
  const uint8_t* iv_expected;
};

class AesCtrEncryptorIvTest : public ::testing::TestWithParam<IvTestCase> {};

TEST_P(AesCtrEncryptorIvTest, IvTest) {
  // Some dummy key and plaintext.
  std::vector<uint8_t> key(16, 1);
  std::vector<uint8_t> plaintext(kTextSizeInBytes, 3);

  std::vector<uint8_t> iv_test(GetParam().iv_test,
                               GetParam().iv_test + GetParam().iv_size);
  std::vector<uint8_t> iv_expected(GetParam().iv_expected,
                                   GetParam().iv_expected + GetParam().iv_size);

  AesCtrEncryptor encryptor;
  ASSERT_TRUE(encryptor.InitializeWithIv(key, iv_test));

  std::vector<uint8_t> encrypted;
  ASSERT_TRUE(encryptor.Crypt(plaintext, &encrypted));
  encryptor.UpdateIv();
  EXPECT_EQ(iv_expected, encryptor.iv());
}

namespace {
// As recommended in ISO/IEC FDIS 23001-7: CENC spec,
// For 64-bit (8-byte) IV_Sizes, initialization vectors for subsequent samples
// can be created by incrementing the initialization vector of the previous
// sample. For 128-bit (16-byte) IV_Sizes, initialization vectors for subsequent
// samples should be created by adding the block count of the previous sample to
// the initialization vector of the previous sample.
const IvTestCase kIvTestCases[] = {
    {kIv128Zero, std::size(kIv128Zero), kIv128Four},
    {kIv128Max64, std::size(kIv128Max64), kIv128OneAndThree},
    {kIv128MaxMinusOne, std::size(kIv128MaxMinusOne), kIv128Two},
    {kIv64Zero, std::size(kIv64Zero), kIv64One},
    {kIv64MaxMinusOne, std::size(kIv64MaxMinusOne), kIv64Max},
    {kIv64Max, std::size(kIv64Max), kIv64Zero}};
}  // namespace

INSTANTIATE_TEST_CASE_P(IvTestCases,
                        AesCtrEncryptorIvTest,
                        ::testing::ValuesIn(kIvTestCases));

class AesCbcTest : public ::testing::Test {
 public:
  AesCbcTest()
      : encryptor_(
            new AesCbcEncryptor(kPkcs5Padding, AesCryptor::kUseConstantIv)),
        decryptor_(
            new AesCbcDecryptor(kPkcs5Padding, AesCryptor::kUseConstantIv)),
        key_(kAesKey, kAesKey + std::size(kAesKey)),
        iv_(kAesIv, kAesIv + std::size(kAesIv)) {}

  void TestEncryptDecrypt(const std::vector<uint8_t>& plaintext,
                          const std::vector<uint8_t>& expected_ciphertext) {
    // Test Vector form.
    TestEncryptDecryptSeparateBuffers(plaintext, expected_ciphertext);
    TestEncryptDecryptInPlace(plaintext, expected_ciphertext);

    // Test string form.
    std::string plaintext_str(plaintext.begin(), plaintext.end());
    std::string expected_ciphertext_str(expected_ciphertext.begin(),
                                        expected_ciphertext.end());
    TestEncryptDecryptSeparateBuffers(plaintext_str, expected_ciphertext_str);
    TestEncryptDecryptInPlace(plaintext_str, expected_ciphertext_str);
  }

 protected:
  template <class T>
  void TestEncryptDecryptSeparateBuffers(const T& plaintext,
                                         const T& expected_ciphertext) {
    ASSERT_TRUE(encryptor_->InitializeWithIv(key_, iv_));
    ASSERT_TRUE(decryptor_->InitializeWithIv(key_, iv_));

    T encrypted;
    ASSERT_TRUE(encryptor_->Crypt(plaintext, &encrypted));
    EXPECT_EQ(expected_ciphertext, encrypted);

    T decrypted;
    ASSERT_TRUE(decryptor_->Crypt(encrypted, &decrypted));
    EXPECT_EQ(plaintext, decrypted);
  }

  template <class T>
  void TestEncryptDecryptInPlace(const T& plaintext,
                                 const T& expected_ciphertext) {
    ASSERT_TRUE(encryptor_->InitializeWithIv(key_, iv_));
    ASSERT_TRUE(decryptor_->InitializeWithIv(key_, iv_));

    T buffer(plaintext);
    ASSERT_TRUE(encryptor_->Crypt(buffer, &buffer));
    EXPECT_EQ(expected_ciphertext, buffer);
    ASSERT_TRUE(decryptor_->Crypt(buffer, &buffer));
    EXPECT_EQ(plaintext, buffer);
  }

  std::unique_ptr<AesCbcEncryptor> encryptor_;
  std::unique_ptr<AesCbcDecryptor> decryptor_;
  std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;
};

TEST_F(AesCbcTest, Aes256CbcPkcs5) {
  // NIST SP 800-38A test vector F.2.5 CBC-AES256.Encrypt.
  static const uint8_t kAesCbcKey[] = {
      0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae,
      0xf0, 0x85, 0x7d, 0x77, 0x81, 0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61,
      0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4};
  static const uint8_t kAesCbcIv[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                      0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                      0x0c, 0x0d, 0x0e, 0x0f};
  static const uint8_t kAesCbcPlaintext[] = {
      // Block #1
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
      // Block #2
      0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
      0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
      // Block #3
      0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
      0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
      // Block #4
      0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
      0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10};
  static const uint8_t kAesCbcCiphertext[] = {
      // Block #1
      0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba,
      0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6,
      // Block #2
      0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d,
      0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d,
      // Block #3
      0x39, 0xf2, 0x33, 0x69, 0xa9, 0xd9, 0xba, 0xcf,
      0xa5, 0x30, 0xe2, 0x63, 0x04, 0x23, 0x14, 0x61,
      // Block #4
      0xb2, 0xeb, 0x05, 0xe2, 0xc3, 0x9b, 0xe9, 0xfc,
      0xda, 0x6c, 0x19, 0x07, 0x8c, 0x6a, 0x9d, 0x1b,
      // PKCS #5 padding, encrypted.
      0x3f, 0x46, 0x17, 0x96, 0xd6, 0xb0, 0xd6, 0xb2,
      0xe0, 0xc2, 0xa7, 0x2b, 0x4d, 0x80, 0xe6, 0x44};

  key_.assign(kAesCbcKey, kAesCbcKey + std::size(kAesCbcKey));
  iv_.assign(kAesCbcIv, kAesCbcIv + std::size(kAesCbcIv));
  const std::vector<uint8_t> plaintext(
      kAesCbcPlaintext, kAesCbcPlaintext + std::size(kAesCbcPlaintext));
  const std::vector<uint8_t> expected_ciphertext(
      kAesCbcCiphertext, kAesCbcCiphertext + std::size(kAesCbcCiphertext));

  TestEncryptDecrypt(plaintext, expected_ciphertext);
}

TEST_F(AesCbcTest, Aes128CbcPkcs5) {
  const std::string kKey = "128=SixteenBytes";
  const std::string kIv = "Sweet Sixteen IV";
  const std::string kPlaintext =
      "Plain text with a g-clef U+1D11E \360\235\204\236";
  const std::string kExpectedCiphertextHex =
      "d4a67a0ba33c30f207344d81d1e944bbe65587c3d7d9939a"
      "c070c62b9c15a3ea312ea4ad1bc7929f4d3c16b03ad5ada8";

  key_.assign(kKey.begin(), kKey.end());
  iv_.assign(kIv.begin(), kIv.end());

  const std::vector<uint8_t> plaintext(kPlaintext.begin(), kPlaintext.end());
  std::string expected_ciphertext_string =
      absl::HexStringToBytes(kExpectedCiphertextHex);
  std::vector<uint8_t> expected_ciphertext(expected_ciphertext_string.begin(),
                                           expected_ciphertext_string.end());
  TestEncryptDecrypt(plaintext, expected_ciphertext);
}

TEST_F(AesCbcTest, Aes192CbcPkcs5) {
  const std::string kKey = "192bitsIsTwentyFourByte!";
  const std::string kIv = "Sweet Sixteen IV";
  const std::string kPlaintext = "Small text";
  const std::string kExpectedCiphertextHex = "78de5d7c2714fc5c61346c5416f6c89a";

  key_.assign(kKey.begin(), kKey.end());
  iv_.assign(kIv.begin(), kIv.end());

  const std::vector<uint8_t> plaintext(kPlaintext.begin(), kPlaintext.end());
  std::string expected_ciphertext_string =
      absl::HexStringToBytes(kExpectedCiphertextHex);
  std::vector<uint8_t> expected_ciphertext(expected_ciphertext_string.begin(),
                                           expected_ciphertext_string.end());
  TestEncryptDecrypt(plaintext, expected_ciphertext);
}

TEST_F(AesCbcTest, NoPaddingNoChainAcrossCalls) {
  const uint8_t kPlaintext[] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
  };
  const uint8_t kCiphertext[] = {
      0x77, 0xcd, 0xe9, 0x1f, 0xe6, 0xdf, 0x9c, 0xbc,
      0x5d, 0x0c, 0x98, 0xf9, 0x6e, 0xfd, 0x59, 0x0b,
  };

  std::vector<uint8_t> plaintext(kPlaintext,
                                 kPlaintext + std::size(kPlaintext));
  std::vector<uint8_t> ciphertext(kCiphertext,
                                  kCiphertext + std::size(kCiphertext));

  AesCbcEncryptor encryptor(kNoPadding, AesCryptor::kUseConstantIv);
  ASSERT_TRUE(encryptor.InitializeWithIv(key_, iv_));

  std::vector<uint8_t> encrypted;
  ASSERT_TRUE(encryptor.Crypt(plaintext, &encrypted));
  EXPECT_EQ(ciphertext, encrypted);
  ASSERT_TRUE(encryptor.Crypt(plaintext, &encrypted));
  EXPECT_EQ(ciphertext, encrypted);

  AesCbcDecryptor decryptor(kNoPadding, AesCryptor::kUseConstantIv);
  ASSERT_TRUE(decryptor.InitializeWithIv(key_, iv_));

  std::vector<uint8_t> decrypted;
  ASSERT_TRUE(decryptor.Crypt(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
  ASSERT_TRUE(decryptor.Crypt(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(AesCbcTest, NoPaddingChainAcrossCalls) {
  const uint8_t kPlaintext[] = {
      0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
      0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
  };
  const uint8_t kCiphertext[] = {
      0x77, 0xcd, 0xe9, 0x1f, 0xe6, 0xdf, 0x9c, 0xbc,
      0x5d, 0x0c, 0x98, 0xf9, 0x6e, 0xfd, 0x59, 0x0b,
  };
  const uint8_t kCiphertext2[] = {
      0xbd, 0xdd, 0xe4, 0x39, 0x52, 0x6f, 0x10, 0x0c,
      0x95, 0x45, 0xc2, 0x74, 0xd4, 0xf7, 0xfd, 0x3f,
  };

  std::vector<uint8_t> plaintext(kPlaintext,
                                 kPlaintext + std::size(kPlaintext));
  std::vector<uint8_t> ciphertext(kCiphertext,
                                  kCiphertext + std::size(kCiphertext));
  std::vector<uint8_t> ciphertext2(kCiphertext2,
                                   kCiphertext2 + std::size(kCiphertext2));

  AesCbcEncryptor encryptor(kNoPadding, AesCryptor::kDontUseConstantIv);
  ASSERT_TRUE(encryptor.InitializeWithIv(key_, iv_));

  std::vector<uint8_t> encrypted;
  ASSERT_TRUE(encryptor.Crypt(plaintext, &encrypted));
  EXPECT_EQ(ciphertext, encrypted);
  // If run encrypt again, the result will be different.
  ASSERT_TRUE(encryptor.Crypt(plaintext, &encrypted));
  EXPECT_NE(ciphertext, ciphertext2);
  EXPECT_EQ(ciphertext2, encrypted);

  AesCbcDecryptor decryptor(kNoPadding, AesCryptor::kDontUseConstantIv);
  ASSERT_TRUE(decryptor.InitializeWithIv(key_, iv_));

  std::vector<uint8_t> decrypted;
  ASSERT_TRUE(decryptor.Crypt(ciphertext, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
  // If run decrypt on ciphertext2 now, it will return the original plaintext.
  ASSERT_TRUE(decryptor.Crypt(ciphertext2, &decrypted));
  EXPECT_EQ(plaintext, decrypted);
}

TEST_F(AesCbcTest, UnsupportedKeySize) {
  EXPECT_FALSE(encryptor_->InitializeWithIv(std::vector<uint8_t>(15, 0), iv_));
  EXPECT_FALSE(decryptor_->InitializeWithIv(std::vector<uint8_t>(15, 0), iv_));
}

TEST_F(AesCbcTest, VariousIvSize) {
  EXPECT_FALSE(encryptor_->InitializeWithIv(key_, std::vector<uint8_t>(14, 0)));
  EXPECT_FALSE(decryptor_->InitializeWithIv(key_, std::vector<uint8_t>(7, 0)));
  EXPECT_FALSE(decryptor_->InitializeWithIv(key_, std::vector<uint8_t>(1, 0)));
  EXPECT_TRUE(decryptor_->InitializeWithIv(key_, std::vector<uint8_t>(8, 0)));
}

TEST_F(AesCbcTest, Pkcs5CipherTextNotMultipleOfBlockSize) {
  std::string plaintext;
  ASSERT_TRUE(decryptor_->InitializeWithIv(key_, iv_));
  EXPECT_FALSE(decryptor_->Crypt("1", &plaintext));
}

TEST_F(AesCbcTest, Pkcs5CipherTextEmpty) {
  std::string plaintext;
  ASSERT_TRUE(decryptor_->InitializeWithIv(key_, iv_));
  EXPECT_FALSE(decryptor_->Crypt("", &plaintext));
}

std::ostream& operator<<(std::ostream& os, CbcPaddingScheme scheme) {
  switch (scheme) {
    case kNoPadding:
      return os << "kNoPadding";
    case kPkcs5Padding:
      return os << "kPkcs5Padding";
    case kCtsPadding:
      return os << "kCtsPadding";
    default:
      return os << "Unrecognized scheme: " << scheme;
  }
}

struct CbcTestCase {
  CbcPaddingScheme padding_scheme;
  const char* plaintext_hex;
  const char* expected_ciphertext_hex;
  friend std::ostream& operator<<(std::ostream& os, const CbcTestCase& param) {
    return os << "padding_scheme = " << param.padding_scheme
              << ", plaintext = " << param.plaintext_hex;
  }
};

const CbcTestCase kCbcTestCases[] = {
    // No padding with zero bytes.
    {kNoPadding, "", ""},
    {kNoPadding,
     "6bc1bee22e409f96e93d7e117393172a6bc1bee22e409f96e93d7e117393172a",
     "77cde91fe6df9cbc5d0c98f96efd590bbddde439526f100c9545c274d4f7fd3f"},
    {kNoPadding,
     "6bc1bee22e409f96e93d7e117393172a6bc1bee22e409f96e93d7e117393172a1234",
     "77cde91fe6df9cbc5d0c98f96efd590bbddde439526f100c9545c274d4f7fd3f1234"},
    // Pkcs5 padding with zero bytes.
    {kPkcs5Padding, "", "f6a3569dea3cda208eb3d5792942612b"},
    // Cts Padding with zero bytes.
    {kCtsPadding, "", ""},
    // Cts Padding with no encrypted blocks.
    {kCtsPadding, "3f593e7a204a5e70f2", "3f593e7a204a5e70f2"},
    // Cts padding with residual bytes.
    {kCtsPadding,
     "e0818f2dc7caaa9edf09285a0c1fca98d39e9b08a47ab6911c4bbdf27d94"
     "f917cdffc9ebb307141f23b0d3921e0ed7f86eb09381286f8e7a4f",
     "b40a0b8704c74e22e8030cad6f272b34ace54cc7c9c64b2018bbcf23df018"
     "39b14899441cf74a9fb2f2b229a609146f31be8e8a826eb6e857e"},
    // Cts padding with even blocks.
    {kCtsPadding,
     "3f593e7a204a5e70f2814dca05aa49d36f2daddc9a24e0515802c539efc3"
     "1094b3ad6c26d6f5c0e387545ce6a4c2c14d",
     "5f32cd0504b27b25ee04090d88d37d340c9c0a9fa50b05358b98fad4302ea"
     "480148d8aa091f4e7d186a7223df153f6f7"},
    // Cts padding with one block and a half.
    {kCtsPadding, "3f593e7a204a5e70f2814dca05aa49d36f2daddc9a4302ea",
     "623fc113fe02ce85628deb58d652c6995f32cd0504b27b25"},
};

class AesCbcCryptorVerificationTest
    : public AesCbcTest,
      public ::testing::WithParamInterface<CbcTestCase> {};

TEST_P(AesCbcCryptorVerificationTest, EncryptDecryptTest) {
  encryptor_.reset(new AesCbcEncryptor(GetParam().padding_scheme,
                                       AesCryptor::kUseConstantIv));
  decryptor_.reset(new AesCbcDecryptor(GetParam().padding_scheme,
                                       AesCryptor::kUseConstantIv));

  std::vector<uint8_t> plaintext;
  std::string plaintext_hex(GetParam().plaintext_hex);
  if (!plaintext_hex.empty()) {
    std::string plaintext_string = absl::HexStringToBytes(plaintext_hex);
    plaintext.assign(plaintext_string.begin(), plaintext_string.end());
  }

  std::vector<uint8_t> expected_ciphertext;
  std::string expected_ciphertext_hex(GetParam().expected_ciphertext_hex);
  if (!expected_ciphertext_hex.empty()) {
    std::string expected_ciphertext_string =
        absl::HexStringToBytes(expected_ciphertext_hex);
    expected_ciphertext.assign(expected_ciphertext_string.begin(),
                               expected_ciphertext_string.end());
  }

  TestEncryptDecrypt(plaintext, expected_ciphertext);
}

INSTANTIATE_TEST_CASE_P(CbcTestCases,
                        AesCbcCryptorVerificationTest,
                        ::testing::ValuesIn(kCbcTestCases));

class AesPerformanceTest : public ::testing::Test {
 public:
  AesPerformanceTest()
      : cbc_encryptor_(kNoPadding, AesCryptor::kUseConstantIv),
        key_(kAesKey, kAesKey + std::size(kAesKey)),
        iv_(kAesIv, kAesIv + std::size(kAesIv)) {}

  void SetUp() override {
    plaintext_.resize(0x10000);
    for (size_t i = 0; i < plaintext_.size(); i++)
      plaintext_[i] = static_cast<uint8_t>(i);
  }

 protected:
  AesCbcEncryptor cbc_encryptor_;
  AesCtrEncryptor ctr_encryptor_;
  std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;
  std::vector<uint8_t> plaintext_;
};

TEST_F(AesPerformanceTest, AesCbc) {
  ASSERT_TRUE(cbc_encryptor_.InitializeWithIv(key_, iv_));
  std::vector<uint8_t> encrypted;
  for (int i = 0; i < 0x100; i++)
    ASSERT_TRUE(cbc_encryptor_.Crypt(plaintext_, &encrypted));
}

TEST_F(AesPerformanceTest, AesCtr) {
  ASSERT_TRUE(ctr_encryptor_.InitializeWithIv(key_, iv_));
  std::vector<uint8_t> encrypted;
  for (int i = 0; i < 0x100; i++)
    ASSERT_TRUE(ctr_encryptor_.Crypt(plaintext_, &encrypted));
}

}  // namespace media
}  // namespace shaka
