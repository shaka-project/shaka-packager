// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// AES Encryptor implementation using openssl.

#ifndef PACKAGER_MEDIA_BASE_AES_ENCRYPTOR_H_
#define PACKAGER_MEDIA_BASE_AES_ENCRYPTOR_H_

#include <string>
#include <vector>

#include "packager/base/macros.h"
#include "packager/media/base/aes_cryptor.h"

namespace shaka {
namespace media {

class AesEncryptor : public AesCryptor {
 public:
  /// @param constant_iv_flag indicates whether a constant iv is used,
  ///        kUseConstantIv means that the same iv is used for all Crypt calls
  ///        until iv is changed via SetIv; otherwise, iv can be incremented
  ///        (for counter mode) or chained (for cipher block chaining mode)
  ///        internally inside Crypt call, i.e. iv will be updated across Crypt
  ///        calls.
  explicit AesEncryptor(ConstantIvFlag constant_iv_flag);
  ~AesEncryptor() override;

  /// Initialize the encryptor with specified key and IV.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AesEncryptor);
};

// Class which implements AES-CTR counter-mode encryption.
class AesCtrEncryptor : public AesEncryptor {
 public:
  AesCtrEncryptor();
  ~AesCtrEncryptor() override;

  uint32_t block_offset() const { return block_offset_; }

 private:
  bool CryptInternal(const uint8_t* plaintext,
                     size_t plaintext_size,
                     uint8_t* ciphertext,
                     size_t* ciphertext_size) override;
  void SetIvInternal() override;

  // Current block offset.
  uint32_t block_offset_;
  // Current AES-CTR counter.
  std::vector<uint8_t> counter_;
  // Encrypted counter.
  std::vector<uint8_t> encrypted_counter_;

  DISALLOW_COPY_AND_ASSIGN(AesCtrEncryptor);
};

enum CbcPaddingScheme {
  // Residual block is left unencrypted.
  kNoPadding,
  // Residual block is padded with pkcs5 and encrypted.
  kPkcs5Padding,
  // Residual block and the next-to-last block are encrypted using ciphertext
  // stealing method.
  kCtsPadding,
};

// Class which implements AES-CBC (Cipher block chaining) encryption.
class AesCbcEncryptor : public AesEncryptor {
 public:
  /// Creates a AesCbcEncryptor with continous cipher block chain across Crypt
  /// calls, i.e. AesCbcEncryptor(padding_scheme, kDontUseConstantIv).
  /// @param padding_scheme indicates the padding scheme used. Currently
  ///        supported schemes: kNoPadding, kPkcs5Padding, kCtsPadding.
  explicit AesCbcEncryptor(CbcPaddingScheme padding_scheme);

  /// @param padding_scheme indicates the padding scheme used. Currently
  ///        supported schemes: kNoPadding, kPkcs5Padding, kCtsPadding.
  /// @param constant_iv_flag indicates whether a constant iv is used,
  ///        kUseConstantIv means that the same iv is used for all Crypt calls
  ///        until iv is changed via SetIv; otherwise, iv is updated internally
  ///        and there is a continuous cipher block chain across Crypt calls
  ///        util iv is changed explicitly via SetIv or UpdateIv functions.
  AesCbcEncryptor(CbcPaddingScheme padding_scheme,
                  ConstantIvFlag constant_iv_flag);

  ~AesCbcEncryptor() override;

 private:
  bool CryptInternal(const uint8_t* plaintext,
                     size_t plaintext_size,
                     uint8_t* ciphertext,
                     size_t* ciphertext_size) override;
  void SetIvInternal() override;
  size_t NumPaddingBytes(size_t size) const override;

  const CbcPaddingScheme padding_scheme_;
  // 16-byte internal iv for crypto operations.
  std::vector<uint8_t> internal_iv_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcEncryptor);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_AES_ENCRYPTOR_H_
