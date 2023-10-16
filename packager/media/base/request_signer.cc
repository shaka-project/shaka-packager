// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/request_signer.h>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <mbedtls/md.h>

#include <packager/media/base/aes_encryptor.h>
#include <packager/media/base/rsa_key.h>

namespace shaka {
namespace media {

RequestSigner::RequestSigner(const std::string& signer_name)
    : signer_name_(signer_name) {}
RequestSigner::~RequestSigner() {}

AesRequestSigner::AesRequestSigner(const std::string& signer_name,
                                   std::unique_ptr<AesCbcEncryptor> encryptor)
    : RequestSigner(signer_name), aes_cbc_encryptor_(std::move(encryptor)) {
  DCHECK(aes_cbc_encryptor_);
}
AesRequestSigner::~AesRequestSigner() {}

AesRequestSigner* AesRequestSigner::CreateSigner(
    const std::string& signer_name,
    const std::vector<uint8_t>& aes_key,
    const std::vector<uint8_t>& iv) {
  std::unique_ptr<AesCbcEncryptor> encryptor(
      new AesCbcEncryptor(kPkcs5Padding, AesCryptor::kUseConstantIv));
  if (!encryptor->InitializeWithIv(aes_key, iv))
    return NULL;
  return new AesRequestSigner(signer_name, std::move(encryptor));
}

bool AesRequestSigner::GenerateSignature(const std::string& message,
                                         std::string* signature) {
  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  DCHECK(md_info);

  std::string hash(mbedtls_md_get_size(md_info), 0);
  CHECK_EQ(0,
           mbedtls_md(md_info, reinterpret_cast<const uint8_t*>(message.data()),
                      message.size(), reinterpret_cast<uint8_t*>(hash.data())));

  return aes_cbc_encryptor_->Crypt(hash, signature);
}

RsaRequestSigner::RsaRequestSigner(
    const std::string& signer_name,
    std::unique_ptr<RsaPrivateKey> rsa_private_key)
    : RequestSigner(signer_name), rsa_private_key_(std::move(rsa_private_key)) {
  DCHECK(rsa_private_key_);
}
RsaRequestSigner::~RsaRequestSigner() {}

RsaRequestSigner* RsaRequestSigner::CreateSigner(
    const std::string& signer_name,
    const std::string& pkcs1_rsa_key) {
  std::unique_ptr<RsaPrivateKey> rsa_private_key(
      RsaPrivateKey::Create(pkcs1_rsa_key));
  if (!rsa_private_key)
    return NULL;
  return new RsaRequestSigner(signer_name, std::move(rsa_private_key));
}

bool RsaRequestSigner::GenerateSignature(const std::string& message,
                                         std::string* signature) {
  return rsa_private_key_->GenerateSignature(message, signature);
}

}  // namespace media
}  // namespace shaka
