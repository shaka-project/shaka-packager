// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_REQUEST_SIGNER_H_
#define PACKAGER_MEDIA_BASE_REQUEST_SIGNER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <packager/macros/classes.h>

namespace shaka {
namespace media {

class AesCbcEncryptor;
class RsaPrivateKey;

/// Abstract class used for signature generation.
class RequestSigner {
 public:
  virtual ~RequestSigner();

  /// Generate signature for the input message.
  /// @param signature should not be NULL.
  /// @return true on success, false otherwise.
  virtual bool GenerateSignature(const std::string& message,
                                 std::string* signature) = 0;

  const std::string& signer_name() const { return signer_name_; }

 protected:
  explicit RequestSigner(const std::string& signer_name);

 private:
  std::string signer_name_;

  DISALLOW_COPY_AND_ASSIGN(RequestSigner);
};

/// AesRequestSigner uses AES-CBC signing.
class AesRequestSigner : public RequestSigner {
 public:
  ~AesRequestSigner() override;

  /// Create an AesSigner object from key and iv.
  /// @return The created AesRequestSigner object on success, NULL otherwise.
  static AesRequestSigner* CreateSigner(const std::string& signer_name,
                                        const std::vector<uint8_t>& aes_key,
                                        const std::vector<uint8_t>& iv);

  /// RequestSigner implementation override.
  bool GenerateSignature(const std::string& message,
                         std::string* signature) override;

 private:
  AesRequestSigner(const std::string& signer_name,
                   std::unique_ptr<AesCbcEncryptor> encryptor);

  std::unique_ptr<AesCbcEncryptor> aes_cbc_encryptor_;

  DISALLOW_COPY_AND_ASSIGN(AesRequestSigner);
};

/// RsaRequestSigner uses RSA-PSS signing.
class RsaRequestSigner : public RequestSigner {
 public:
  ~RsaRequestSigner() override;

  /// Create an RsaSigner object using a DER encoded PKCS#1 RSAPrivateKey.
  /// @return The created RsaRequestSigner object on success, NULL otherwise.
  static RsaRequestSigner* CreateSigner(const std::string& signer_name,
                                        const std::string& pkcs1_rsa_key);

  /// RequestSigner implementation override.
  bool GenerateSignature(const std::string& message,
                         std::string* signature) override;

 private:
  RsaRequestSigner(const std::string& signer_name,
                   std::unique_ptr<RsaPrivateKey> rsa_private_key);

  std::unique_ptr<RsaPrivateKey> rsa_private_key_;

  DISALLOW_COPY_AND_ASSIGN(RsaRequestSigner);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_REQUEST_SIGNER_H_
