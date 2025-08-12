// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CRYPTO_SAMPLE_AES_EC3_CRYPTOR_H_
#define PACKAGER_MEDIA_CRYPTO_SAMPLE_AES_EC3_CRYPTOR_H_

#include <cstdint>

#include <packager/media/base/aes_cryptor.h>

namespace shaka {
namespace media {

/// Implements SAMPLE-AES E-AC3 encryption / decryption per specification at:
/// https://goo.gl/1sgcwY.
class SampleAesEc3Cryptor : public AesCryptor {
 public:
  /// @param cryptor points to an AesCryptor instance which performs the actual
  ///        encryption/decryption. Note that @a cryptor shall not use constant
  ///        iv.
  explicit SampleAesEc3Cryptor(std::unique_ptr<AesCryptor> cryptor);

  /// @name AesCryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;
  /// @}

 private:
  SampleAesEc3Cryptor(const SampleAesEc3Cryptor&) = delete;
  SampleAesEc3Cryptor& operator=(const SampleAesEc3Cryptor&) = delete;

  // AesCryptor implementation overrides.
  bool CryptInternal(const uint8_t* text,
                     size_t text_size,
                     uint8_t* crypt_text,
                     size_t* crypt_text_size) override;
  void SetIvInternal() override;

  std::unique_ptr<AesCryptor> cryptor_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CRYPTO_SAMPLE_AES_EC3_CRYPTOR_H_
