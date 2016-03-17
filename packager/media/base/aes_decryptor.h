// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// AES Decryptor implementation using openssl.

#ifndef MEDIA_BASE_AES_DECRYPTOR_H_
#define MEDIA_BASE_AES_DECRYPTOR_H_

#include <string>
#include <vector>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/base/stl_util.h"
#include "packager/media/base/aes_encryptor.h"

struct aes_key_st;
typedef struct aes_key_st AES_KEY;

namespace edash_packager {
namespace media {

class AesDecryptor {
 public:
  AesDecryptor();
  virtual ~AesDecryptor();

  virtual bool InitializeWithIv(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& iv) = 0;

  /// @name Various forms of decrypt calls.
  /// The plaintext and ciphertext pointers can be the same address.
  /// @{
  virtual bool Decrypt(const uint8_t* ciphertext,
                       size_t ciphertext_size,
                       uint8_t* plaintext) = 0;

  virtual bool Decrypt(const std::vector<uint8_t>& ciphertext,
                       std::vector<uint8_t>* plaintext) = 0;

  virtual bool Decrypt(const std::string& ciphertext,
                       std::string* plaintext) = 0;
  /// @}

  /// Set IV. @a block_offset_ is reset to 0 on success.
  /// @return true if successful, false if the input is invalid.
  virtual bool SetIv(const std::vector<uint8_t>& iv) = 0;

  const std::vector<uint8_t>& iv() const { return iv_; }

 protected:
  // Initialization vector, with size 8 or 16.
  std::vector<uint8_t> iv_;
  // Openssl AES_KEY.
  scoped_ptr<AES_KEY> aes_key_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AesDecryptor);
};

// Class which implements AES-CTR counter-mode decryption.
class AesCtrDecryptor : public AesDecryptor {
 public:
  AesCtrDecryptor();
  ~AesCtrDecryptor() override;

  /// @name AesDecryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

  bool Decrypt(const uint8_t* ciphertext,
               size_t ciphertext_size,
               uint8_t* plaintext) override;

  bool Decrypt(const std::vector<uint8_t>& ciphertext,
               std::vector<uint8_t>* plaintext) override;

  bool Decrypt(const std::string& ciphertext, std::string* plaintext) override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

  uint32_t block_offset() const { return encryptor_->block_offset(); }

 private:
  scoped_ptr<AesCtrEncryptor> encryptor_;

  DISALLOW_COPY_AND_ASSIGN(AesCtrDecryptor);
};

// Class which implements AES-CBC (Cipher block chaining) decryption with
// PKCS#5 padding.
class AesCbcPkcs5Decryptor : public AesDecryptor {
 public:
  AesCbcPkcs5Decryptor();
  ~AesCbcPkcs5Decryptor() override;

  /// @name AesDecryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

  bool Decrypt(const uint8_t* ciphertext,
               size_t ciphertext_size,
               uint8_t* plaintext) override;

  bool Decrypt(const std::vector<uint8_t>& ciphertext,
               std::vector<uint8_t>* plaintext) override;

  bool Decrypt(const std::string& ciphertext, std::string* plaintext) override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

 private:
  DISALLOW_COPY_AND_ASSIGN(AesCbcPkcs5Decryptor);
};

// Class which implements AES-CBC (Cipher block chaining) decryption with
// Ciphertext stealing.
class AesCbcCtsDecryptor : public AesDecryptor {
 public:
  AesCbcCtsDecryptor();
  ~AesCbcCtsDecryptor() override;

  /// @name AesDecryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

  bool Decrypt(const uint8_t* ciphertext,
               size_t ciphertext_size,
               uint8_t* plaintext) override;

  bool Decrypt(const std::vector<uint8_t>& ciphertext,
               std::vector<uint8_t>* plaintext) override;

  bool Decrypt(const std::string& ciphertext, std::string* plaintext) override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

 private:
  DISALLOW_COPY_AND_ASSIGN(AesCbcCtsDecryptor);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_AES_DECRYPTOR_H_
