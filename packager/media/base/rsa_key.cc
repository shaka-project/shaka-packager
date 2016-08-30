// Copyright 2014 Google Inc. All rights reserved.
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

#include "packager/media/base/rsa_key.h"

#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <vector>

#include "packager/base/logging.h"
#include "packager/base/sha1.h"

namespace {

const size_t kPssSaltLength = 20u;

// Serialize rsa key from DER encoded PKCS#1 RSAPrivateKey.
RSA* DeserializeRsaKey(const std::string& serialized_key,
                       bool deserialize_private_key) {
  if (serialized_key.empty()) {
    LOG(ERROR) << "Serialized RSA Key is empty.";
    return NULL;
  }

  BIO* bio = BIO_new_mem_buf(const_cast<char*>(serialized_key.data()),
                             serialized_key.size());
  if (bio == NULL) {
    LOG(ERROR) << "BIO_new_mem_buf returned NULL.";
    return NULL;
  }
  RSA* rsa_key = deserialize_private_key ? d2i_RSAPrivateKey_bio(bio, NULL)
                                         : d2i_RSAPublicKey_bio(bio, NULL);
  BIO_free(bio);
  return rsa_key;
}

RSA* DeserializeRsaPrivateKey(const std::string& serialized_key) {
  RSA* rsa_key = DeserializeRsaKey(serialized_key, true);
  if (!rsa_key) {
    LOG(ERROR) << "Private RSA key deserialization failure.";
    return NULL;
  }
  if (RSA_check_key(rsa_key) != 1) {
    LOG(ERROR) << "Invalid RSA Private key: " << ERR_error_string(
                                                     ERR_get_error(), NULL);
    RSA_free(rsa_key);
    return NULL;
  }
  return rsa_key;
}

RSA* DeserializeRsaPublicKey(const std::string& serialized_key) {
  RSA* rsa_key = DeserializeRsaKey(serialized_key, false);
  if (!rsa_key) {
    LOG(ERROR) << "Private RSA key deserialization failure.";
    return NULL;
  }
  if (RSA_size(rsa_key) <= 0) {
    LOG(ERROR) << "Invalid RSA Public key: " << ERR_error_string(
                                                    ERR_get_error(), NULL);
    RSA_free(rsa_key);
    return NULL;
  }
  return rsa_key;
}

}  // namespace

namespace shaka {
namespace media {

RsaPrivateKey::RsaPrivateKey(RSA* rsa_key) : rsa_key_(rsa_key) {
  DCHECK(rsa_key);
}
RsaPrivateKey::~RsaPrivateKey() {
  if (rsa_key_ != NULL)
    RSA_free(rsa_key_);
}

RsaPrivateKey* RsaPrivateKey::Create(const std::string& serialized_key) {
  RSA* rsa_key = DeserializeRsaPrivateKey(serialized_key);
  return rsa_key == NULL ? NULL : new RsaPrivateKey(rsa_key);
}

bool RsaPrivateKey::Decrypt(const std::string& encrypted_message,
                            std::string* decrypted_message) {
  DCHECK(decrypted_message);

  size_t rsa_size = RSA_size(rsa_key_);
  if (encrypted_message.size() != rsa_size) {
    LOG(ERROR) << "Encrypted RSA message has the wrong size (expected "
               << rsa_size << ", actual " << encrypted_message.size() << ").";
    return false;
  }

  decrypted_message->resize(rsa_size);
  int decrypted_size = RSA_private_decrypt(
      rsa_size, reinterpret_cast<const uint8_t*>(encrypted_message.data()),
      reinterpret_cast<uint8_t*>(&(*decrypted_message)[0]), rsa_key_,
      RSA_PKCS1_OAEP_PADDING);

  if (decrypted_size == -1) {
    LOG(ERROR) << "RSA private decrypt failure: " << ERR_error_string(
                                                         ERR_get_error(), NULL);
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

  std::string message_digest = base::SHA1HashString(message);

  // Add PSS padding.
  size_t rsa_size = RSA_size(rsa_key_);
  std::vector<uint8_t> padded_digest(rsa_size);
  if (!RSA_padding_add_PKCS1_PSS_mgf1(
          rsa_key_, &padded_digest[0],
          reinterpret_cast<uint8_t*>(&message_digest[0]), EVP_sha1(),
          EVP_sha1(), kPssSaltLength)) {
    LOG(ERROR) << "RSA padding failure: " << ERR_error_string(ERR_get_error(),
                                                              NULL);
    return false;
  }

  // Encrypt PSS padded digest.
  signature->resize(rsa_size);
  int signature_size = RSA_private_encrypt(
      padded_digest.size(), &padded_digest[0],
      reinterpret_cast<uint8_t*>(&(*signature)[0]), rsa_key_, RSA_NO_PADDING);

  if (signature_size != static_cast<int>(rsa_size)) {
    LOG(ERROR) << "RSA private encrypt failure: " << ERR_error_string(
                                                         ERR_get_error(), NULL);
    return false;
  }
  return true;
}

RsaPublicKey::RsaPublicKey(RSA* rsa_key) : rsa_key_(rsa_key) {
  DCHECK(rsa_key);
}
RsaPublicKey::~RsaPublicKey() {
  if (rsa_key_ != NULL)
    RSA_free(rsa_key_);
}

RsaPublicKey* RsaPublicKey::Create(const std::string& serialized_key) {
  RSA* rsa_key = DeserializeRsaPublicKey(serialized_key);
  return rsa_key == NULL ? NULL : new RsaPublicKey(rsa_key);
}

bool RsaPublicKey::Encrypt(const std::string& clear_message,
                           std::string* encrypted_message) {
  DCHECK(encrypted_message);
  if (clear_message.empty()) {
    LOG(ERROR) << "Message to be encrypted is empty.";
    return false;
  }

  size_t rsa_size = RSA_size(rsa_key_);
  encrypted_message->resize(rsa_size);
  int encrypted_size =
      RSA_public_encrypt(clear_message.size(),
                         reinterpret_cast<const uint8_t*>(clear_message.data()),
                         reinterpret_cast<uint8_t*>(&(*encrypted_message)[0]),
                         rsa_key_, RSA_PKCS1_OAEP_PADDING);

  if (encrypted_size != static_cast<int>(rsa_size)) {
    LOG(ERROR) << "RSA public encrypt failure: " << ERR_error_string(
                                                        ERR_get_error(), NULL);
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

  size_t rsa_size = RSA_size(rsa_key_);
  if (signature.size() != rsa_size) {
    LOG(ERROR) << "Message signature is of the wrong size (expected "
               << rsa_size << ", actual " << signature.size() << ").";
    return false;
  }

  // Decrypt the signature.
  std::vector<uint8_t> padded_digest(signature.size());
  int decrypted_size =
      RSA_public_decrypt(signature.size(),
                         reinterpret_cast<const uint8_t*>(signature.data()),
                         &padded_digest[0],
                         rsa_key_,
                         RSA_NO_PADDING);

  if (decrypted_size != static_cast<int>(rsa_size)) {
    LOG(ERROR) << "RSA public decrypt failure: " << ERR_error_string(
                                                        ERR_get_error(), NULL);
    return false;
  }

  std::string message_digest = base::SHA1HashString(message);

  // Verify PSS padding.
  return RSA_verify_PKCS1_PSS_mgf1(
             rsa_key_,
             reinterpret_cast<const uint8_t*>(message_digest.data()),
             EVP_sha1(),
             EVP_sha1(),
             &padded_digest[0],
             kPssSaltLength) != 0;
}

}  // namespace media
}  // namespace shaka
