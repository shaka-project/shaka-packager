// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Unit test for rsa_key RSA encryption and signing.

#include <packager/media/base/rsa_key.h>

#include <filesystem>
#include <memory>

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <packager/media/base/test/rsa_test_data.h>
#include <packager/media/test/test_data_util.h>

namespace shaka {
namespace media {

namespace {

class RsaKeyTest : public ::testing::TestWithParam<RsaTestSet> {
 public:
  RsaKeyTest() : test_set_(GetParam()) {}

  void SetUp() override {
    private_key_.reset(RsaPrivateKey::Create(test_set_.private_key));
    ASSERT_TRUE(private_key_ != NULL);

    public_key_.reset(RsaPublicKey::Create(test_set_.public_key));
    ASSERT_TRUE(public_key_ != NULL);
  }

 protected:
  const RsaTestSet& test_set_;
  std::unique_ptr<RsaPrivateKey> private_key_;
  std::unique_ptr<RsaPublicKey> public_key_;
};

TEST_P(RsaKeyTest, BadPublicKey) {
  std::unique_ptr<RsaPublicKey> public_key(
      RsaPublicKey::Create("bad_public_key"));
  EXPECT_TRUE(public_key == NULL);
}

TEST_P(RsaKeyTest, BadPrivateKey) {
  std::unique_ptr<RsaPrivateKey> private_key(
      RsaPrivateKey::Create("bad_private_key"));
  EXPECT_TRUE(private_key == NULL);
}

TEST_P(RsaKeyTest, LoadPublicKey) {
  std::unique_ptr<RsaPublicKey> public_key(
      RsaPublicKey::Create(test_set_.public_key));
  EXPECT_TRUE(public_key != NULL);
}

TEST_P(RsaKeyTest, LoadPrivateKey) {
  std::unique_ptr<RsaPrivateKey> private_key(
      RsaPrivateKey::Create(test_set_.private_key));
  EXPECT_TRUE(private_key != NULL);
}

TEST_P(RsaKeyTest, LoadPublicKeyInPrivateKey) {
  std::unique_ptr<RsaPrivateKey> private_key(
      RsaPrivateKey::Create(test_set_.public_key));
  EXPECT_TRUE(private_key == NULL);
}

TEST_P(RsaKeyTest, LoadPrivateKeyInPublicKey) {
  std::unique_ptr<RsaPublicKey> public_key(
      RsaPublicKey::Create(test_set_.private_key));
  EXPECT_TRUE(public_key == NULL);
}

TEST_P(RsaKeyTest, EncryptAndDecrypt) {
  std::string encrypted_message;
  ASSERT_TRUE(public_key_->Encrypt(test_set_.test_message, &encrypted_message));

  std::string decrypted_message;
  EXPECT_TRUE(private_key_->Decrypt(encrypted_message, &decrypted_message));
  EXPECT_EQ(test_set_.test_message, decrypted_message);
}

TEST_P(RsaKeyTest, DecryptGoldenMessage) {
  // This message is from an older version that predates our use of mbedtls,
  // but proves that the new system is compatible with the messages produced by
  // the old one.
  std::string decrypted_message;
  EXPECT_TRUE(
      private_key_->Decrypt(test_set_.encrypted_message, &decrypted_message));
  EXPECT_EQ(test_set_.test_message, decrypted_message);
}

TEST_P(RsaKeyTest, BadEncMessage1) {
  // Add a byte to the encrypted message.
  std::string bad_enc_message = test_set_.encrypted_message + '\0';

  std::string decrypted_message;
  EXPECT_FALSE(private_key_->Decrypt(bad_enc_message, &decrypted_message));
}

TEST_P(RsaKeyTest, BadEncMessage2) {
  // Remove a byte from the encrypted message.
  std::string bad_enc_message = test_set_.encrypted_message;
  bad_enc_message.erase(bad_enc_message.end() - 1);

  std::string decrypted_message;
  EXPECT_FALSE(private_key_->Decrypt(bad_enc_message, &decrypted_message));
}

TEST_P(RsaKeyTest, BadEncMessage3) {
  // Change a byte in the encrypted message.
  std::string bad_enc_message = test_set_.encrypted_message;
  bad_enc_message[bad_enc_message.size() / 2] ^= 0x55;

  std::string decrypted_message;
  EXPECT_FALSE(private_key_->Decrypt(bad_enc_message, &decrypted_message));
}

TEST_P(RsaKeyTest, SignAndVerify) {
  std::string signature;
  ASSERT_TRUE(
      private_key_->GenerateSignature(test_set_.test_message, &signature));
  EXPECT_TRUE(public_key_->VerifySignature(test_set_.test_message, signature));
}

TEST_P(RsaKeyTest, VerifyGoldenSignature) {
  // This signature is from an older version that predates our use of mbedtls,
  // but proves that the new system is compatible with the signatures produced
  // by the old one.
  EXPECT_TRUE(public_key_->VerifySignature(test_set_.test_message,
                                           test_set_.signature));
}

TEST_P(RsaKeyTest, BadSignature1) {
  // Add a byte to the signature.
  std::string bad_signature = test_set_.signature + '\0';
  EXPECT_FALSE(
      public_key_->VerifySignature(test_set_.test_message, bad_signature));
}

TEST_P(RsaKeyTest, BadSignature2) {
  // Remove a byte from the signature.
  std::string bad_signature = test_set_.signature;
  bad_signature.erase(bad_signature.end() - 1);

  EXPECT_FALSE(
      public_key_->VerifySignature(test_set_.test_message, bad_signature));
}

TEST_P(RsaKeyTest, BadSignature3) {
  // Change a byte in the signature.
  std::string bad_signature = test_set_.signature;
  bad_signature[bad_signature.size() / 2] ^= 0x55;

  EXPECT_FALSE(
      public_key_->VerifySignature(test_set_.test_message, bad_signature));
}

INSTANTIATE_TEST_CASE_P(RsaTestKeys,
                        RsaKeyTest,
                        ::testing::Values(RsaTestData().test_set_3072_bits(),
                                          RsaTestData().test_set_2048_bits()));

}  // namespace
}  // namespace media
}  // namespace shaka
