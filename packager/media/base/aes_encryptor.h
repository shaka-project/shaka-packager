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

#include "packager/base/memory/scoped_ptr.h"
#include "packager/base/stl_util.h"

struct aes_key_st;
typedef struct aes_key_st AES_KEY;

namespace edash_packager {
namespace media {

class AesEncryptor {
 public:
  AesEncryptor();
  virtual ~AesEncryptor();

  /// Initialize the encryptor with specified key and a random generated IV
  /// of the specified size.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithRandomIv(const std::vector<uint8_t>& key,
                                      uint8_t iv_size);

  /// Initialize the encryptor with specified key and IV.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv);

  /// @name Various forms of encrypt calls.
  /// The plaintext and ciphertext pointers can be the same address.
  bool Encrypt(const std::vector<uint8_t>& plaintext,
               std::vector<uint8_t>* ciphertext);
  bool Encrypt(const std::string& plaintext, std::string* ciphertext);
  bool Encrypt(const uint8_t* plaintext,
               size_t plaintext_size,
               uint8_t* ciphertext) {
    return EncryptInternal(plaintext, plaintext_size, ciphertext);
  }
  /// @}

  /// Update IV for next sample.
  /// As recommended in ISO/IEC FDIS 23001-7:
  /// IV need to be updated per sample for CENC.
  virtual void UpdateIv() = 0;

  /// Set IV.
  /// @return true if successful, false if the input is invalid.
  virtual bool SetIv(const std::vector<uint8_t>& iv) = 0;

  /// @return The current iv.
  const std::vector<uint8_t>& iv() const { return iv_; }

 protected:
  /// Internal implementation of encrypt function.
  /// @param plaintext points to the input plaintext.
  /// @param plaintext_size is the size of input plaintext.
  /// @param[out] ciphertext points to the output ciphertext. @a plaintext and
  ///             @a ciphertext can point to the same address.
  virtual bool EncryptInternal(const uint8_t* plaintext,
                               size_t plaintext_size,
                               uint8_t* ciphertext) = 0;
  /// @param size specifies the input plaintext size.
  /// @returns The number of padding bytes needed for output ciphertext.
  virtual size_t NumPaddingBytes(size_t size) const = 0;

  void set_iv(const std::vector<uint8_t>& iv) { iv_ = iv; }
  AES_KEY* aes_key() const { return aes_key_.get(); }

 private:
  // Initialization vector, with size 8 or 16.
  std::vector<uint8_t> iv_;
  // Openssl AES_KEY.
  scoped_ptr<AES_KEY> aes_key_;

  DISALLOW_COPY_AND_ASSIGN(AesEncryptor);
};

// Class which implements AES-CTR counter-mode encryption.
class AesCtrEncryptor : public AesEncryptor {
 public:
  AesCtrEncryptor();
  ~AesCtrEncryptor() override;

  /// @name AesEncryptor implementation overrides.
  /// @{
  /// Update IV for next sample. @a block_offset_ is reset to 0.
  /// As recommended in ISO/IEC FDIS 23001-7: CENC spec,
  ///   For 64-bit IV size, new_iv = old_iv + 1;
  ///   For 128-bit IV size, new_iv = old_iv + previous_sample_block_count.
  void UpdateIv() override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

  uint32_t block_offset() const { return block_offset_; }

 protected:
  bool EncryptInternal(const uint8_t* plaintext,
                       size_t plaintext_size,
                       uint8_t* ciphertext) override;
  size_t NumPaddingBytes(size_t size) const override;

 private:
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

  /// @name AesEncryptor implementation overrides.
  /// @{
  void UpdateIv() override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

 protected:
  bool EncryptInternal(const uint8_t* plaintext,
                       size_t plaintext_size,
                       uint8_t* ciphertext) override;
  size_t NumPaddingBytes(size_t size) const override;

 private:
  const CbcPaddingScheme padding_scheme_;
  const bool chain_across_calls_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcEncryptor);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_AES_ENCRYPTOR_H_
