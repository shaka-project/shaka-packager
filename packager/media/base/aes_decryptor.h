// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// AES Decryptor implementation using mbedtls.

#ifndef PACKAGER_MEDIA_BASE_AES_DECRYPTOR_H_
#define PACKAGER_MEDIA_BASE_AES_DECRYPTOR_H_

#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/aes_cryptor.h>
#include <packager/media/base/aes_encryptor.h>

namespace shaka {
namespace media {

/// For AES-CTR, encryption and decryption are identical.
using AesCtrDecryptor = AesCtrEncryptor;

/// Class which implements AES-CBC (Cipher block chaining) decryption.
class AesCbcDecryptor : public AesCryptor {
 public:
  /// Creates a AesCbcDecryptor with continous cipher block chain across Crypt
  /// calls.
  /// @param padding_scheme indicates the padding scheme used. Currently
  ///        supported schemes: kNoPadding, kPkcs5Padding, kCtsPadding.
  explicit AesCbcDecryptor(CbcPaddingScheme padding_scheme);

  /// @param padding_scheme indicates the padding scheme used. Currently
  ///        supported schemes: kNoPadding, kPkcs5Padding, kCtsPadding.
  /// @param constant_iv_flag indicates whether a constant iv is used,
  ///        kUseConstantIv means that the same iv is used for all Crypt calls
  ///        until iv is changed via SetIv; otherwise, iv is updated internally
  ///        and there is a continuous cipher block chain across Crypt calls
  ///        util iv is changed explicitly via SetIv or UpdateIv functions.
  AesCbcDecryptor(CbcPaddingScheme padding_scheme,
                  ConstantIvFlag constant_iv_flag);

  ~AesCbcDecryptor() override;

  /// @name AesCryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

  size_t RequiredOutputSize(size_t plaintext_size) override;
  /// @}

 private:
  bool CryptInternal(const uint8_t* ciphertext,
                     size_t ciphertext_size,
                     uint8_t* plaintext,
                     size_t* plaintext_size) override;
  void SetIvInternal() override;
  void CbcDecryptBlocks(const uint8_t* plaintext,
                        size_t plaintext_size,
                        uint8_t* ciphertext);

  const CbcPaddingScheme padding_scheme_;
  // 16-byte internal iv for crypto operations.
  std::vector<uint8_t> internal_iv_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcDecryptor);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_AES_DECRYPTOR_H_
