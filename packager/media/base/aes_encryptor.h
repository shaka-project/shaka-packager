// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// AES Encryptor implementation using openssl.

#ifndef MEDIA_BASE_AES_ENCRYPTOR_H_
#define MEDIA_BASE_AES_ENCRYPTOR_H_

#include <string>
#include <vector>

#include "packager/base/macros.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/aes_cryptor.h"

namespace edash_packager {
namespace media {

class AesEncryptor : public AesCryptor {
 public:
  AesEncryptor();
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

  /// @name AesCryptor implementation overrides.
  /// @{
  /// Update IV for next sample. @a block_offset_ is reset to 0.
  /// As recommended in ISO/IEC FDIS 23001-7: CENC spec,
  ///   For 64-bit IV size, new_iv = old_iv + 1;
  ///   For 128-bit IV size, new_iv = old_iv + previous_sample_block_count.
  void UpdateIv() override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

  uint32_t block_offset() const { return block_offset_; }

 private:
  bool CryptInternal(const uint8_t* plaintext,
                     size_t plaintext_size,
                     uint8_t* ciphertext,
                     size_t* ciphertext_size) override;

  // Current block offset.
  uint32_t block_offset_;
  // Current AES-CTR counter.
  std::vector<uint8_t> counter_;
  // Encrypted counter.
  std::vector<uint8_t> encrypted_counter_;
  // Keep track of whether the counter has overflowed.
  bool counter_overflow_;

  DISALLOW_COPY_AND_ASSIGN(AesCtrEncryptor);
};

enum CbcPaddingScheme {
  kNoPadding,
  kPkcs5Padding,
  kCtsPadding,
};

const bool kChainAcrossCalls = true;

// Class which implements AES-CBC (Cipher block chaining) encryption.
class AesCbcEncryptor : public AesEncryptor {
 public:
  /// @param padding_scheme indicates the padding scheme used. Currently
  ///        supported schemes: kNoPadding, kPkcs5Padding, kCtsPadding.
  /// @param chain_across_calls indicates whether there is a continuous cipher
  ///        block chain across calls for Encrypt function. If it is false, iv
  ///        is not updated across Encrypt function calls.
  AesCbcEncryptor(CbcPaddingScheme padding_scheme, bool chain_across_calls);
  ~AesCbcEncryptor() override;

  /// @name AesCryptor implementation overrides.
  /// @{
  void UpdateIv() override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

 private:
  bool CryptInternal(const uint8_t* plaintext,
                     size_t plaintext_size,
                     uint8_t* ciphertext,
                     size_t* ciphertext_size) override;
  size_t NumPaddingBytes(size_t size) const override;

  const CbcPaddingScheme padding_scheme_;
  const bool chain_across_calls_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcEncryptor);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_AES_ENCRYPTOR_H_
