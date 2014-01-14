// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_REQUEST_SIGNER_H_
#define MEDIA_BASE_REQUEST_SIGNER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace media {

class AesCbcEncryptor;
class RsaPrivateKey;

// Define an abstract signer class for signature generation.
class RequestSigner {
 public:
  virtual ~RequestSigner();

  // Generate signature for |message|. |signature| should not be NULL.
  // Return true on success.
  virtual bool GenerateSignature(const std::string& message,
                                 std::string* signature) = 0;

  const std::string& signer_name() const { return signer_name_; }

 protected:
  explicit RequestSigner(const std::string& signer_name);

 private:
  std::string signer_name_;

  DISALLOW_COPY_AND_ASSIGN(RequestSigner);
};

// AesRequestSigner uses AES-CBC signing.
class AesRequestSigner : public RequestSigner {
 public:
  virtual ~AesRequestSigner();

  // Create an AesSigner object from key and iv in hex.
  // Return NULL on failure.
  static AesRequestSigner* CreateSigner(const std::string& signer_name,
                                        const std::string& aes_key_hex,
                                        const std::string& iv_hex);

  virtual bool GenerateSignature(const std::string& message,
                                 std::string* signature) OVERRIDE;

 private:
  AesRequestSigner(const std::string& signer_name,
                   scoped_ptr<AesCbcEncryptor> encryptor);

  scoped_ptr<AesCbcEncryptor> aes_cbc_encryptor_;

  DISALLOW_COPY_AND_ASSIGN(AesRequestSigner);
};

// RsaRequestSigner uses RSA-PSS signing.
class RsaRequestSigner : public RequestSigner {
 public:
  virtual ~RsaRequestSigner();

  // Create an RsaSigner object using a DER encoded PKCS#1 RSAPrivateKey.
  // Return NULL on failure.
  static RsaRequestSigner* CreateSigner(const std::string& signer_name,
                                        const std::string& pkcs1_rsa_key);

  virtual bool GenerateSignature(const std::string& message,
                                 std::string* signature) OVERRIDE;

 private:
  RsaRequestSigner(const std::string& signer_name,
                   scoped_ptr<RsaPrivateKey> rsa_private_key);

  scoped_ptr<RsaPrivateKey> rsa_private_key_;

  DISALLOW_COPY_AND_ASSIGN(RsaRequestSigner);
};

}  // namespace media

#endif  // MEDIA_BASE_REQUEST_SIGNER_H_
