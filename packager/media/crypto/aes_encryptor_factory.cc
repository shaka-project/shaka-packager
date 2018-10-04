// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/crypto/aes_encryptor_factory.h"

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/aes_pattern_cryptor.h"
#include "packager/media/crypto/sample_aes_ec3_cryptor.h"

namespace shaka {
namespace media {

std::unique_ptr<AesCryptor> AesEncryptorFactory::CreateEncryptor(
    FourCC protection_scheme,
    uint8_t crypt_byte_block,
    uint8_t skip_byte_block,
    Codec codec,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv) {
  std::unique_ptr<AesCryptor> encryptor;
  switch (protection_scheme) {
    case FOURCC_cenc:
      encryptor.reset(new AesCtrEncryptor);
      break;
    case FOURCC_cbc1:
      encryptor.reset(new AesCbcEncryptor(kNoPadding));
      break;
    case FOURCC_cens:
      encryptor.reset(new AesPatternCryptor(
          crypt_byte_block, skip_byte_block,
          AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
          AesCryptor::kDontUseConstantIv,
          std::unique_ptr<AesCryptor>(new AesCtrEncryptor)));
      break;
    case FOURCC_cbcs:
      encryptor.reset(new AesPatternCryptor(
          crypt_byte_block, skip_byte_block,
          AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
          AesCryptor::kUseConstantIv,
          std::unique_ptr<AesCryptor>(new AesCbcEncryptor(kNoPadding))));
      break;
    case kAppleSampleAesProtectionScheme:
      if (crypt_byte_block == 0 && skip_byte_block == 0) {
        if (codec == kCodecEAC3) {
          encryptor.reset(new SampleAesEc3Cryptor(
              std::unique_ptr<AesCryptor>(new AesCbcEncryptor(kNoPadding))));
        } else {
          encryptor.reset(
              new AesCbcEncryptor(kNoPadding, AesCryptor::kUseConstantIv));
        }
      } else {
        encryptor.reset(new AesPatternCryptor(
            crypt_byte_block, skip_byte_block,
            AesPatternCryptor::kSkipIfCryptByteBlockRemaining,
            AesCryptor::kUseConstantIv,
            std::unique_ptr<AesCryptor>(new AesCbcEncryptor(kNoPadding))));
      }
      break;
    default:
      LOG(ERROR) << "Unsupported protection scheme.";
      return nullptr;
  }

  if (iv.empty()) {
    std::vector<uint8_t> random_iv;
    if (!AesCryptor::GenerateRandomIv(protection_scheme, &random_iv)) {
      LOG(ERROR) << "Failed to generate random iv.";
      return nullptr;
    }
    if (!encryptor->InitializeWithIv(key, random_iv))
      return nullptr;
  } else {
    if (!encryptor->InitializeWithIv(key, iv))
      return nullptr;
  }

  return encryptor;
}

}  // namespace media
}  // namespace shaka
