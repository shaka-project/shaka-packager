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
  virtual bool InitializeWithRandomIv(const std::vector<uint8_t>& key,
                                      uint8_t iv_size);

  /// Initialize the encryptor with specified key and IV.
  /// @return true on successful initialization, false otherwise.
  virtual bool InitializeWithIv(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& iv) = 0;

  virtual size_t NumPaddingBytes(size_t size) = 0;

  /// @name Various forms of encrypt and decrypt calls.
  /// The plaintext and ciphertext pointers can be the same address.
  /// @{
  virtual bool EncryptData(const uint8_t* plaintext,
                           size_t plaintext_size,
                           uint8_t* ciphertext) = 0;

  bool Encrypt(const std::vector<uint8_t>& plaintext,
               std::vector<uint8_t>* ciphertext);

  bool Encrypt(const std::string& plaintext, std::string* ciphertext);
  /// @}

  /// Update IV for next sample.
  /// As recommended in ISO/IEC FDIS 23001-7:
  /// IV need to be updated per sample for CENC.
  /// IV need not be unique per sample for CBC mode.
  virtual void UpdateIv() = 0;

  /// Set IV.
  /// @return true if successful, false if the input is invalid.
  virtual bool SetIv(const std::vector<uint8_t>& iv) = 0;

  const std::vector<uint8_t>& iv() const { return iv_; }

 protected:
  // Initialization vector, with size 8 or 16.
  std::vector<uint8_t> iv_;
  // Openssl AES_KEY.
  scoped_ptr<AES_KEY> aes_key_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AesEncryptor);
};

// Class which implements AES-CTR counter-mode encryption/decryption.
class AesCtrEncryptor : public AesEncryptor {
 public:
  AesCtrEncryptor();
  ~AesCtrEncryptor() override;

  /// @name AesEncryptor implementation overrides.
  /// @{
  /// @param key should be 16 bytes in size as specified in CENC spec.
  /// @param iv_size should be either 8 or 16 as specified in CENC spec.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

  size_t NumPaddingBytes(size_t size) override;

  bool EncryptData(const uint8_t* plaintext,
                   size_t plaintext_size,
                   uint8_t* ciphertext) override;

  /// Update IV for next sample. @a block_offset_ is reset to 0.
  /// As recommended in ISO/IEC FDIS 23001-7: CENC spec,
  ///   For 64-bit IV size, new_iv = old_iv + 1;
  ///   For 128-bit IV size, new_iv = old_iv + previous_sample_block_count.
  void UpdateIv() override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

  uint32_t block_offset() const { return block_offset_; }

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

// Class which implements AES-CBC (Cipher block chaining) encryption with
// PKCS#5 padding.
class AesCbcPkcs5Encryptor : public AesEncryptor {
 public:
  AesCbcPkcs5Encryptor();
  ~AesCbcPkcs5Encryptor() override;

  /// @name AesEncryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

  size_t NumPaddingBytes(size_t size) override;

  bool EncryptData(const uint8_t* plaintext,
                   size_t plaintext_size,
                   uint8_t* ciphertext) override;

  void UpdateIv() override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

 private:
  DISALLOW_COPY_AND_ASSIGN(AesCbcPkcs5Encryptor);
};

// Class which implements AES-CBC (Cipher block chaining) encryption with
// Ciphertext stealing.
class AesCbcCtsEncryptor : public AesEncryptor {
 public:
  AesCbcCtsEncryptor();
  ~AesCbcCtsEncryptor() override;

  /// @name AesEncryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

  size_t NumPaddingBytes(size_t size) override;

  bool EncryptData(const uint8_t* plaintext,
                   size_t plaintext_size,
                   uint8_t* ciphertext) override;

  void UpdateIv() override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

 private:
  DISALLOW_COPY_AND_ASSIGN(AesCbcCtsEncryptor);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_AES_ENCRYPTOR_H_
