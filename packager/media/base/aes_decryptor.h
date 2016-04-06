// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// AES Decryptor implementation using openssl.

#ifndef PACKAGER_MEDIA_BASE_AES_DECRYPTOR_H_
#define PACKAGER_MEDIA_BASE_AES_DECRYPTOR_H_

#include <vector>

#include "packager/base/macros.h"
#include "packager/media/base/aes_cryptor.h"
#include "packager/media/base/aes_encryptor.h"

namespace edash_packager {
namespace media {

/// For AES-CTR, encryption and decryption are identical.
using AesCtrDecryptor = AesCtrEncryptor;

/// Class which implements AES-CBC (Cipher block chaining) decryption.
class AesCbcDecryptor : public AesCryptor {
 public:
  /// @param padding_scheme indicates the padding scheme used. Currently
  ///        supported schemes: kNoPadding, kPkcs5Padding, kCtsPadding.
  /// @param chain_across_calls indicates whether there is a continuous cipher
  ///        block chain across calls for Decrypt function. If it is false, iv
  ///        is not updated across Decrypt function calls.
  AesCbcDecryptor(CbcPaddingScheme padding_scheme, bool chain_across_calls);
  ~AesCbcDecryptor() override;

  /// @name AesCryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;
  bool SetIv(const std::vector<uint8_t>& iv) override;
  void UpdateIv() override {
    // Nop for decryptor.
  }
  /// @}

 private:
  bool CryptInternal(const uint8_t* ciphertext,
                     size_t ciphertext_size,
                     uint8_t* plaintext,
                     size_t* plaintext_size) override;

  const CbcPaddingScheme padding_scheme_;
  const bool chain_across_calls_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcDecryptor);
};

}  // namespace media
}  // namespace edash_packager

#endif  // PACKAGER_MEDIA_BASE_AES_DECRYPTOR_H_
