// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Declaration of classes representing RSA private and public keys used
// for message signing, signature verification, encryption and decryption.

#ifndef PACKAGER_MEDIA_BASE_RSA_KEY_H_
#define PACKAGER_MEDIA_BASE_RSA_KEY_H_

#include <string>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>

#include <packager/macros/classes.h>

namespace shaka {
namespace media {

/// Rsa private key, used for message signing and decryption.
class RsaPrivateKey {
 public:
  ~RsaPrivateKey();

  /// Create an RsaPrivateKey object using a DER encoded PKCS#1 RSAPrivateKey.
  /// @return The created RsaPrivateKey object on success, NULL otherwise.
  static RsaPrivateKey* Create(const std::string& serialized_key);

  /// Decrypt a message using RSA-OAEP.
  /// @param decrypted_message must not be NULL.
  /// @return true if successful, false otherwise.
  bool Decrypt(const std::string& encrypted_message,
               std::string* decrypted_message);

  /// Generate RSASSA-PSS signature.
  /// @param signature must not be NULL.
  /// @return true if successful, false otherwise.
  bool GenerateSignature(const std::string& message, std::string* signature);

 private:
  RsaPrivateKey();

  bool Deserialize(const std::string& serialized_key);

  mbedtls_pk_context pk_context_;
  mbedtls_entropy_context entropy_context_;
  mbedtls_ctr_drbg_context prng_context_;

  DISALLOW_COPY_AND_ASSIGN(RsaPrivateKey);
};

/// Rsa public key, used for signature verification and encryption.
class RsaPublicKey {
 public:
  ~RsaPublicKey();

  /// Create an RsaPublicKey object using a DER encoded PKCS#1 RSAPublicKey.
  /// @return The created RsaPrivateKey object on success, NULL otherwise.
  static RsaPublicKey* Create(const std::string& serialized_key);

  /// Encrypt a message using RSA-OAEP.
  /// @param encrypted_message must not be NULL.
  /// @return true if successful, false otherwise.
  bool Encrypt(const std::string& clear_message,
               std::string* encrypted_message);

  /// Verify RSASSA-PSS signature.
  /// @return true if verification succeeds, false otherwise.
  bool VerifySignature(const std::string& message,
                       const std::string& signature);

 private:
  RsaPublicKey();

  bool Deserialize(const std::string& serialized_key);

  mbedtls_pk_context pk_context_;
  mbedtls_entropy_context entropy_context_;
  mbedtls_ctr_drbg_context prng_context_;

  DISALLOW_COPY_AND_ASSIGN(RsaPublicKey);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_RSA_KEY_H_
