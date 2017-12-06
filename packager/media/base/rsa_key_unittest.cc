// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Unit test for rsa_key RSA encryption and signing.

#include <gtest/gtest.h>
#include <memory>
#include "packager/media/base/rsa_key.h"
#include "packager/media/base/test/fake_prng.h"
#include "packager/media/base/test/rsa_test_data.h"

namespace {
// BoringSSL does not support RAND_set_rand_method yet, so we cannot use fake
// prng with boringssl.
const bool kIsFakePrngSupported = false;
}  // namespace

namespace shaka {
namespace media {

class RsaKeyTest : public ::testing::TestWithParam<RsaTestSet> {
 public:
  RsaKeyTest() : test_set_(GetParam()) {}

  void SetUp() override {
    if (kIsFakePrngSupported) {
      // Make OpenSSL RSA deterministic.
      ASSERT_TRUE(fake_prng::StartFakePrng());
    }

    private_key_.reset(RsaPrivateKey::Create(test_set_.private_key));
    ASSERT_TRUE(private_key_ != NULL);
    public_key_.reset(RsaPublicKey::Create(test_set_.public_key));
    ASSERT_TRUE(public_key_ != NULL);
  }
  void TearDown() override {
    if (kIsFakePrngSupported)
      fake_prng::StopFakePrng();
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
  EXPECT_TRUE(public_key_->Encrypt(test_set_.test_message, &encrypted_message));
  if (kIsFakePrngSupported) {
    EXPECT_EQ(test_set_.encrypted_message, encrypted_message);
  }

  std::string decrypted_message;
  EXPECT_TRUE(private_key_->Decrypt(encrypted_message, &decrypted_message));
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
  EXPECT_TRUE(
      private_key_->GenerateSignature(test_set_.test_message, &signature));
  if (kIsFakePrngSupported) {
    EXPECT_EQ(test_set_.signature, signature);
  }
  EXPECT_TRUE(public_key_->VerifySignature(test_set_.test_message, signature));
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

}  // namespace media
}  // namespace shaka
