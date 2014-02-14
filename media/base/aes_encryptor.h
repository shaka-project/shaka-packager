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

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"

struct aes_key_st;
typedef struct aes_key_st AES_KEY;

namespace media {

class AesCtrEncryptor {
 public:
  AesCtrEncryptor();
  ~AesCtrEncryptor();

  // Initialize the encryptor with specified key. A random iv will be generated.
  // Return false if either the key or the initialization vector cannot be
  // used. A valid |key| should be 16 bytes in size. A valid |iv_size| should be
  // either 8 or 16.
  // |block_offset_| is set to 0.
  bool InitializeWithRandomIv(const std::vector<uint8>& key, uint8 iv_size);

  // Initialize the encryptor with specified key and iv. Return false if either
  // the key or the initialization vector cannot be used. A valid |key| should
  // be 16 bytes in size. A valid |iv| should be 8 bytes or 16 bytes in size.
  // |block_offset_| is set to 0.
  bool InitializeWithIv(const std::vector<uint8>& key,
                        const std::vector<uint8>& iv);

  // Various forms of encrypt calls. |block_offset_| will be updated according
  // to input plaintext size.
  bool Encrypt(const uint8* plaintext,
               size_t plaintext_size,
               uint8* ciphertext);

  bool Encrypt(const std::vector<uint8>& plaintext,
               std::vector<uint8>* ciphertext) {
    ciphertext->resize(plaintext.size());
    return Encrypt(&plaintext[0], plaintext.size(), &(*ciphertext)[0]);
  }

  bool Encrypt(const std::string& plaintext, std::string* ciphertext) {
    ciphertext->resize(plaintext.size());
    return Encrypt(reinterpret_cast<const uint8*>(plaintext.data()),
                   plaintext.size(),
                   reinterpret_cast<uint8*>(string_as_array(ciphertext)));
  }

  // For AES CTR, encryption and decryption are identical.
  bool Decrypt(const uint8* ciphertext,
               size_t ciphertext_size,
               uint8* plaintext) {
    return Encrypt(ciphertext, ciphertext_size, plaintext);
  }

  bool Decrypt(const std::vector<uint8>& ciphertext,
               std::vector<uint8>* plaintext) {
    return Encrypt(ciphertext, plaintext);
  }

  bool Decrypt(const std::string& ciphertext, std::string* plaintext) {
    return Encrypt(ciphertext, plaintext);
  }

  // Update IV for next sample. |block_offset_| is reset to 0.
  // As recommended in ISO/IEC FDIS 23001-7: CENC spec,
  //   For 64-bit IV size, new_iv = old_iv + 1;
  //   For 128-bit IV size, new_iv = old_iv + previous_sample_block_count.
  void UpdateIv();

  // Set IV. |block_offset_| is reset to 0. Return false if |iv| is invalid.
  bool SetIv(const std::vector<uint8>& iv);

  const std::vector<uint8>& iv() const { return iv_; }

  uint32 block_offset() const { return block_offset_; }

 private:
  // Initialization vector, with size 8 or 16.
  std::vector<uint8> iv_;
  // Current block offset.
  uint32 block_offset_;
  // Openssl AES_KEY.
  scoped_ptr<AES_KEY> aes_key_;
  // Current AES-CTR counter.
  std::vector<uint8> counter_;
  // Encrypted counter.
  std::vector<uint8> encrypted_counter_;
  // Keep track of whether the counter has overflowed.
  bool counter_overflow_;

  DISALLOW_COPY_AND_ASSIGN(AesCtrEncryptor);
};

class AesCbcEncryptor {
 public:
  AesCbcEncryptor();
  ~AesCbcEncryptor();

  // Initialize the encryptor with specified key and iv.
  // Return false if either the key or the initialization vector cannot be used.
  // A valid |key| should be 128 bits or 192 bits or 256 bits in size as defined
  // in AES. A valid |iv| should be 16 bytes in size.
  bool InitializeWithIv(const std::vector<uint8>& key,
                        const std::vector<uint8>& iv);

  // |plaintext| will be PKCS5 padded before being encrypted.
  void Encrypt(const std::string& plaintext, std::string* ciphertext);

  // Set IV. Return false if |iv| is invalid.
  bool SetIv(const std::vector<uint8>& iv);

  const std::vector<uint8>& iv() const { return iv_; }

 private:
  std::vector<uint8> iv_;
  scoped_ptr<AES_KEY> encrypt_key_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcEncryptor);
};

class AesCbcDecryptor {
 public:
  AesCbcDecryptor();
  ~AesCbcDecryptor();

  // Initialize the decryptor with specified key and iv.
  // Return false if either the key or the initialization vector cannot be used.
  // A valid |key| should be 128 bits or 192 bits or 256 bits in size as defined
  // in AES. A valid |iv| should be 16 bytes in size.
  bool InitializeWithIv(const std::vector<uint8>& key,
                        const std::vector<uint8>& iv);

  // We expect |ciphertext| generated with PKCS5 padding. Return false it not.
  bool Decrypt(const std::string& ciphertext, std::string* plaintext);

  // Set IV. Return false if |iv| is invalid.
  bool SetIv(const std::vector<uint8>& iv);

  const std::vector<uint8>& iv() const { return iv_; }

 private:
  std::vector<uint8> iv_;
  scoped_ptr<AES_KEY> decrypt_key_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcDecryptor);
};

}  // namespace media

#endif  // MEDIA_BASE_AES_ENCRYPTOR_H_
