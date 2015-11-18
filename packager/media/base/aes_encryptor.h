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

// Class which implements AES-CTR counter-mode encryption/decryption.
class AesCtrEncryptor {
 public:
  AesCtrEncryptor();
  ~AesCtrEncryptor();

  /// Initialize the encryptor with specified key and a random generated IV
  /// of the specified size. block_offset() is reset to 0 on success.
  /// @param key should be 16 bytes in size as specified in CENC spec.
  /// @param iv_size should be either 8 or 16 as specified in CENC spec.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithRandomIv(const std::vector<uint8_t>& key, uint8_t iv_size);

  /// Initialize the encryptor with specified key and IV. block_offset() is
  /// reset to 0 on success.
  /// @param key should be 16 bytes in size as specified in CENC spec.
  /// @param iv should be 8 bytes or 16 bytes in size as specified in CENC spec.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv);

  /// @name Various forms of encrypt calls.
  /// block_offset() will be updated according to input plaintext size.
  /// The plaintext and ciphertext pointers can be the same address.
  /// @{
  bool Encrypt(const uint8_t* plaintext,
               size_t plaintext_size,
               uint8_t* ciphertext);

  bool Encrypt(const std::vector<uint8_t>& plaintext,
               std::vector<uint8_t>* ciphertext) {
    ciphertext->resize(plaintext.size());
    return Encrypt(&plaintext[0], plaintext.size(), &(*ciphertext)[0]);
  }

  bool Encrypt(const std::string& plaintext, std::string* ciphertext) {
    ciphertext->resize(plaintext.size());
    return Encrypt(reinterpret_cast<const uint8_t*>(plaintext.data()),
                   plaintext.size(),
                   reinterpret_cast<uint8_t*>(string_as_array(ciphertext)));
  }
  /// @}

  // For AES CTR, encryption and decryption are identical.
  bool Decrypt(const uint8_t* ciphertext,
               size_t ciphertext_size,
               uint8_t* plaintext) {
    return Encrypt(ciphertext, ciphertext_size, plaintext);
  }

  bool Decrypt(const std::vector<uint8_t>& ciphertext,
               std::vector<uint8_t>* plaintext) {
    return Encrypt(ciphertext, plaintext);
  }

  bool Decrypt(const std::string& ciphertext, std::string* plaintext) {
    return Encrypt(ciphertext, plaintext);
  }

  /// Update IV for next sample. @a block_offset_ is reset to 0.
  /// As recommended in ISO/IEC FDIS 23001-7: CENC spec,
  ///   For 64-bit IV size, new_iv = old_iv + 1;
  ///   For 128-bit IV size, new_iv = old_iv + previous_sample_block_count.
  void UpdateIv();

  /// Set IV. @a block_offset_ is reset to 0 on success.
  /// @return true if successful, false if the input is invalid.
  bool SetIv(const std::vector<uint8_t>& iv);

  const std::vector<uint8_t>& iv() const { return iv_; }

  uint32_t block_offset() const { return block_offset_; }

 private:
  // Initialization vector, with size 8 or 16.
  std::vector<uint8_t> iv_;
  // Current block offset.
  uint32_t block_offset_;
  // Openssl AES_KEY.
  scoped_ptr<AES_KEY> aes_key_;
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
class AesCbcPkcs5Encryptor {
 public:
  AesCbcPkcs5Encryptor();
  ~AesCbcPkcs5Encryptor();

  /// Initialize the encryptor with specified key and IV.
  /// @param key should be 128 bits or 192 bits or 256 bits in size as defined
  ///        in AES spec.
  /// @param iv should be 16 bytes in size.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv);

  /// @param plaintext will be PKCS5 padded before being encrypted.
  /// @param ciphertext should not be NULL.
  void Encrypt(const std::string& plaintext, std::string* ciphertext);

  /// @return true if successful, false if the input is invalid.
  bool SetIv(const std::vector<uint8_t>& iv);

  const std::vector<uint8_t>& iv() const { return iv_; }

 private:
  std::vector<uint8_t> iv_;
  scoped_ptr<AES_KEY> encrypt_key_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcPkcs5Encryptor);
};

// Class which implements AES-CBC (Cipher block chaining) decryption with
// PKCS#5 padding.
class AesCbcPkcs5Decryptor {
 public:
  AesCbcPkcs5Decryptor();
  ~AesCbcPkcs5Decryptor();

  /// Initialize the decryptor with specified key and IV.
  /// @param key should be 128 bits or 192 bits or 256 bits in size as defined
  ///        in AES spec.
  /// @param iv should be 16 bytes in size.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv);

  /// @param ciphertext is expected to be padded with PKCS5 padding.
  /// @param plaintext should not be NULL.
  /// @return true on success, false otherwise.
  bool Decrypt(const std::string& ciphertext, std::string* plaintext);

  /// @return true if successful, false if the input is invalid.
  bool SetIv(const std::vector<uint8_t>& iv);

  const std::vector<uint8_t>& iv() const { return iv_; }

 private:
  std::vector<uint8_t> iv_;
  scoped_ptr<AES_KEY> decrypt_key_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcPkcs5Decryptor);
};

// Class which implements AES-CBC (Cipher block chaining) encryption with
// Ciphertext stealing.
class AesCbcCtsEncryptor {
 public:
  AesCbcCtsEncryptor();
  ~AesCbcCtsEncryptor();

  /// Initialize the encryptor with specified key and IV.
  /// @param key should be 128 bits or 192 bits or 256 bits in size as defined
  ///        in AES spec.
  /// @param iv should be 16 bytes in size.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv);

  /// @param plaintext points to the data to be encrypted.
  /// @param size is the number of bytes to be encrypted. If less than 16
  ///        bytes, it will be copied in the clear.
  /// @param ciphertext should not be NULL. The buffer should be at least
  ///        @a size bytes in length.
  void Encrypt(const uint8_t* plaintext, size_t size, uint8_t* ciphertext);

  /// @param plaintext contains the data to be encrypted. If less than 16
  ///        bytes in size, it will be copied in the clear.
  /// @param ciphertext should not be NULL. Caller retains ownership.
  void Encrypt(const std::vector<uint8_t>& plaintext,
               std::vector<uint8_t>* ciphertext);

  /// @param iv is the initialization vector. Should be 16 bytes in size.
  /// @return true if successful, false if the input is invalid.
  bool SetIv(const std::vector<uint8_t>& iv);

  const std::vector<uint8_t>& iv() const { return iv_; }

 private:
  std::vector<uint8_t> iv_;
  scoped_ptr<AES_KEY> encrypt_key_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcCtsEncryptor);
};

// Class which implements AES-CBC (Cipher block chaining) decryption with
// Ciphertext stealing.
class AesCbcCtsDecryptor {
 public:
  AesCbcCtsDecryptor();
  ~AesCbcCtsDecryptor();

  /// Initialize the decryptor with specified key and IV.
  /// @param key should be 128 bits or 192 bits or 256 bits in size as defined
  ///        in AES spec.
  /// @param iv should be 16 bytes in size.
  /// @return true on successful initialization, false otherwise.
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv);

  /// @param ciphertext points to the data to be decrypted.
  /// @param size is the number of bytes to be decrypted. If less than 16
  ///        bytes, it will be copied in the clear.
  /// @param plaintext should not be NULL. The buffer should be at least
  ///        @a size bytes in length.
  void Decrypt(const uint8_t* ciphertext, size_t size, uint8_t* plaintext);

  /// @param ciphertext contains the data to be decrypted. If less than 16
  ///        bytes in size, it will be copied in the clear.
  /// @param plaintext should not be NULL. Caller retains ownership.
  void Decrypt(const std::vector<uint8_t>& ciphertext,
               std::vector<uint8_t>* plaintext);

  /// @return true if successful, false if the input is invalid.
  bool SetIv(const std::vector<uint8_t>& iv);

  const std::vector<uint8_t>& iv() const { return iv_; }

 private:
  std::vector<uint8_t> iv_;
  scoped_ptr<AES_KEY> decrypt_key_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcCtsDecryptor);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_AES_ENCRYPTOR_H_
