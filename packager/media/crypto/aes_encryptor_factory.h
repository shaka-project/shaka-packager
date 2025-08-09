// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CRYPTO_AES_ENCRYPTOR_FACTORY_H_
#define PACKAGER_MEDIA_CRYPTO_AES_ENCRYPTOR_FACTORY_H_

#include <cstdint>

#include <packager/media/base/fourccs.h>
#include <packager/media/base/stream_info.h>

namespace shaka {
namespace media {

class AesCryptor;

/// A factory class to create encryptors.
class AesEncryptorFactory {
 public:
  AesEncryptorFactory() = default;
  virtual ~AesEncryptorFactory() = default;

  // Virtual for mocking.
  virtual std::unique_ptr<AesCryptor> CreateEncryptor(
      FourCC protection_scheme,
      uint8_t crypt_byte_block,
      uint8_t skip_byte_block,
      Codec codec,
      const std::vector<uint8_t>& key,
      const std::vector<uint8_t>& iv);

 private:
  AesEncryptorFactory(const AesEncryptorFactory&) = delete;
  AesEncryptorFactory& operator=(const AesEncryptorFactory&) = delete;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CRYPTO_AES_ENCRYPTOR_FACTORY_H_
