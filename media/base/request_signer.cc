// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/request_signer.h"

#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/aes_encryptor.h"
#include "media/base/rsa_key.h"

namespace media {

RequestSigner::RequestSigner(const std::string& signer_name)
    : signer_name_(signer_name) {}
RequestSigner::~RequestSigner() {}

AesRequestSigner::AesRequestSigner(const std::string& signer_name,
                                   scoped_ptr<AesCbcEncryptor> encryptor)
    : RequestSigner(signer_name), aes_cbc_encryptor_(encryptor.Pass()) {
  DCHECK(aes_cbc_encryptor_);
}
AesRequestSigner::~AesRequestSigner() {}

AesRequestSigner* AesRequestSigner::CreateSigner(const std::string& signer_name,
                                                 const std::string& aes_key_hex,
                                                 const std::string& iv_hex) {
  std::vector<uint8> aes_key;
  if (!base::HexStringToBytes(aes_key_hex, &aes_key)) {
    LOG(ERROR) << "Failed to convert hex string to bytes: " << aes_key_hex;
    return NULL;
  }
  std::vector<uint8> iv;
  if (!base::HexStringToBytes(iv_hex, &iv)) {
    LOG(ERROR) << "Failed to convert hex string to bytes: " << iv_hex;
    return NULL;
  }

  scoped_ptr<AesCbcEncryptor> encryptor(new AesCbcEncryptor());
  if (!encryptor->InitializeWithIv(aes_key, iv))
    return NULL;
  return new AesRequestSigner(signer_name, encryptor.Pass());
}

bool AesRequestSigner::GenerateSignature(const std::string& message,
                                         std::string* signature) {
  aes_cbc_encryptor_->Encrypt(base::SHA1HashString(message), signature);
  return true;
}

RsaRequestSigner::RsaRequestSigner(const std::string& signer_name,
                                   scoped_ptr<RsaPrivateKey> rsa_private_key)
    : RequestSigner(signer_name), rsa_private_key_(rsa_private_key.Pass()) {
  DCHECK(rsa_private_key_);
}
RsaRequestSigner::~RsaRequestSigner() {}

RsaRequestSigner* RsaRequestSigner::CreateSigner(
    const std::string& signer_name,
    const std::string& pkcs1_rsa_key) {
  scoped_ptr<RsaPrivateKey> rsa_private_key(
      RsaPrivateKey::Create(pkcs1_rsa_key));
  if (!rsa_private_key)
    return NULL;
  return new RsaRequestSigner(signer_name, rsa_private_key.Pass());
}

bool RsaRequestSigner::GenerateSignature(const std::string& message,
                                         std::string* signature) {
  return rsa_private_key_->GenerateSignature(message, signature);
}

}  // namespace media
