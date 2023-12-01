// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// RSA signature details:
//   Algorithm: RSASSA-PSS
//   Hash algorithm: SHA1
//   Mask generation function: mgf1SHA1
//   Salt length: 20 bytes
//   Trailer field: 0xbc
//
// RSA encryption details:
//   Algorithm: RSA-OAEP
//   Mask generation function: mgf1SHA1
//   Label (encoding paramter): empty std::string

#include <packager/media/base/rsa_key.h>

#include <memory>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>

namespace {

const size_t kPssSaltLength = 20u;

std::string mbedtls_strerr(int rv) {
  // There is always a "high level" error.
  std::string output(mbedtls_high_level_strerr(rv));

  // Some errors have a "low level" error, which is like an inner error code
  // with a deeper explanation.  But on mac and Windows, ostream crashes if you
  // give it NULL.  So we combine them ourselves with a NULL check.
  const char* low_level_error = mbedtls_low_level_strerr(rv);
  if (low_level_error) {
    output += ": ";
    output += low_level_error;
  }

  return output;
}

std::string sha1(const std::string& message) {
  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  DCHECK(md_info);

  std::string hash(mbedtls_md_get_size(md_info), 0);
  CHECK_EQ(0,
           mbedtls_md(md_info, reinterpret_cast<const uint8_t*>(message.data()),
                      message.size(), reinterpret_cast<uint8_t*>(hash.data())));

  return hash;
}

}  // namespace

namespace shaka {
namespace media {

RsaPrivateKey::RsaPrivateKey() {
  mbedtls_pk_init(&pk_context_);
  mbedtls_entropy_init(&entropy_context_);
  mbedtls_ctr_drbg_init(&prng_context_);
}

RsaPrivateKey::~RsaPrivateKey() {
  mbedtls_pk_free(&pk_context_);
  mbedtls_entropy_free(&entropy_context_);
  mbedtls_ctr_drbg_free(&prng_context_);
}

RsaPrivateKey* RsaPrivateKey::Create(const std::string& serialized_key) {
  std::unique_ptr<RsaPrivateKey> key(new RsaPrivateKey());
  if (!key->Deserialize(serialized_key)) {
    return NULL;
  }
  return key.release();
}

bool RsaPrivateKey::Deserialize(const std::string& serialized_key) {
  const mbedtls_pk_info_t* pk_info = mbedtls_pk_info_from_type(MBEDTLS_PK_RSA);
  DCHECK(pk_info);

  CHECK_EQ(mbedtls_ctr_drbg_seed(&prng_context_, mbedtls_entropy_func,
                                 &entropy_context_, /* custom= */ NULL,
                                 /* custom_len= */ 0),
           0);

  int rv = mbedtls_pk_parse_key(
      &pk_context_, reinterpret_cast<const uint8_t*>(serialized_key.data()),
      serialized_key.size(),
      /* password= */ NULL,
      /* password_len= */ 0, mbedtls_ctr_drbg_random, &prng_context_);
  if (rv != 0) {
    LOG(ERROR) << "RSA private key failed to load: " << mbedtls_strerr(rv);
    return false;
  }

  // Set the padding mode and digest mode.
  mbedtls_rsa_context* rsa_context = mbedtls_pk_rsa(pk_context_);
  rv = mbedtls_rsa_set_padding(rsa_context, MBEDTLS_RSA_PKCS_V21,
                               MBEDTLS_MD_SHA1);
  if (rv != 0) {
    LOG(ERROR) << "RSA private key failed to set padding: "
               << mbedtls_strerr(rv);
    return false;
  }

  return true;
}

bool RsaPrivateKey::Decrypt(const std::string& encrypted_message,
                            std::string* decrypted_message) {
  DCHECK(decrypted_message);

  mbedtls_rsa_context* rsa_context = mbedtls_pk_rsa(pk_context_);

  size_t rsa_size = mbedtls_rsa_get_len(rsa_context);
  if (encrypted_message.size() != rsa_size) {
    LOG(ERROR) << "Encrypted RSA message has the wrong size (expected "
               << rsa_size << ", actual " << encrypted_message.size() << ").";
    return false;
  }
  decrypted_message->resize(encrypted_message.size());

  size_t decrypted_size = 0;
  int rv = mbedtls_rsa_rsaes_oaep_decrypt(
      rsa_context, mbedtls_ctr_drbg_random, &prng_context_,
      /* label= */ NULL,
      /* label_len= */ 0, &decrypted_size,
      reinterpret_cast<const uint8_t*>(encrypted_message.data()),
      reinterpret_cast<uint8_t*>(decrypted_message->data()),
      decrypted_message->size());

  if (rv != 0) {
    LOG(ERROR) << "RSA private decrypt failure: " << mbedtls_strerr(rv);
    return false;
  }
  decrypted_message->resize(decrypted_size);
  return true;
}

bool RsaPrivateKey::GenerateSignature(const std::string& message,
                                      std::string* signature) {
  DCHECK(signature);
  if (message.empty()) {
    LOG(ERROR) << "Message to be signed is empty.";
    return false;
  }

  mbedtls_rsa_context* rsa_context = mbedtls_pk_rsa(pk_context_);

  size_t rsa_size = mbedtls_rsa_get_len(rsa_context);
  signature->resize(rsa_size);

  std::string hash = sha1(message);
  int rv = mbedtls_rsa_rsassa_pss_sign_ext(
      rsa_context, mbedtls_ctr_drbg_random, &prng_context_, MBEDTLS_MD_SHA1,
      static_cast<unsigned int>(hash.size()),
      reinterpret_cast<const uint8_t*>(hash.data()), kPssSaltLength,
      reinterpret_cast<uint8_t*>(signature->data()));

  if (rv != 0) {
    LOG(ERROR) << "RSA sign failure: " << mbedtls_strerr(rv);
    return false;
  }
  return true;
}

RsaPublicKey::RsaPublicKey() {
  mbedtls_pk_init(&pk_context_);
  mbedtls_entropy_init(&entropy_context_);
  mbedtls_ctr_drbg_init(&prng_context_);
}

RsaPublicKey::~RsaPublicKey() {
  mbedtls_pk_free(&pk_context_);
  mbedtls_entropy_free(&entropy_context_);
  mbedtls_ctr_drbg_free(&prng_context_);
}

RsaPublicKey* RsaPublicKey::Create(const std::string& serialized_key) {
  std::unique_ptr<RsaPublicKey> key(new RsaPublicKey());
  if (!key->Deserialize(serialized_key)) {
    return NULL;
  }
  return key.release();
}

bool RsaPublicKey::Deserialize(const std::string& serialized_key) {
  const mbedtls_pk_info_t* pk_info = mbedtls_pk_info_from_type(MBEDTLS_PK_RSA);
  DCHECK(pk_info);

  CHECK_EQ(mbedtls_ctr_drbg_seed(&prng_context_, mbedtls_entropy_func,
                                 &entropy_context_, /* custom= */ NULL,
                                 /* custom_len= */ 0),
           0);

  int rv = mbedtls_pk_parse_public_key(
      &pk_context_, reinterpret_cast<const uint8_t*>(serialized_key.data()),
      serialized_key.size());
  if (rv != 0) {
    LOG(ERROR) << "RSA public key failed to load: " << mbedtls_strerr(rv);
    return false;
  }

  // Set the padding mode and digest mode.
  mbedtls_rsa_context* rsa_context = mbedtls_pk_rsa(pk_context_);
  rv = mbedtls_rsa_set_padding(rsa_context, MBEDTLS_RSA_PKCS_V21,
                               MBEDTLS_MD_SHA1);
  if (rv != 0) {
    LOG(ERROR) << "RSA public key failed to set padding: "
               << mbedtls_strerr(rv);
    return false;
  }

  return true;
}

bool RsaPublicKey::Encrypt(const std::string& clear_message,
                           std::string* encrypted_message) {
  DCHECK(encrypted_message);
  if (clear_message.empty()) {
    LOG(ERROR) << "Message to be encrypted is empty.";
    return false;
  }

  mbedtls_rsa_context* rsa_context = mbedtls_pk_rsa(pk_context_);

  size_t rsa_size = mbedtls_rsa_get_len(rsa_context);
  encrypted_message->resize(rsa_size);

  int rv = mbedtls_rsa_rsaes_oaep_encrypt(
      rsa_context, mbedtls_ctr_drbg_random, &prng_context_,
      /* label= */ NULL,
      /* label_len= */ 0, clear_message.size(),
      reinterpret_cast<const uint8_t*>(clear_message.data()),
      reinterpret_cast<uint8_t*>(encrypted_message->data()));

  if (rv != 0) {
    LOG(ERROR) << "RSA public encrypt failure: " << mbedtls_strerr(rv);
    return false;
  }
  return true;
}

bool RsaPublicKey::VerifySignature(const std::string& message,
                                   const std::string& signature) {
  if (message.empty()) {
    LOG(ERROR) << "Signed message is empty.";
    return false;
  }

  mbedtls_rsa_context* rsa_context = mbedtls_pk_rsa(pk_context_);

  size_t rsa_size = mbedtls_rsa_get_len(rsa_context);
  if (signature.size() != rsa_size) {
    LOG(ERROR) << "Message signature is of the wrong size (expected "
               << rsa_size << ", actual " << signature.size() << ").";
    return false;
  }

  // Verify the signature.
  std::string hash = sha1(message);
  int rv = mbedtls_rsa_rsassa_pss_verify_ext(
      rsa_context, MBEDTLS_MD_SHA1, static_cast<unsigned int>(hash.size()),
      reinterpret_cast<const uint8_t*>(hash.data()), MBEDTLS_MD_SHA1,
      kPssSaltLength, reinterpret_cast<const uint8_t*>(signature.data()));

  if (rv != 0) {
    LOG(ERROR) << "RSA signature verification failed: " << mbedtls_strerr(rv);
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace shaka
