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

  /// Initialize the decryptor with specified key and IV.
  /// @return true on successful initialization, false otherwise.
  virtual bool InitializeWithIv(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& iv) = 0;

  /// @name Various forms of decrypt calls.
  /// The plaintext and ciphertext pointers can be the same address.
  /// @{
  bool Decrypt(const std::vector<uint8_t>& ciphertext,
               std::vector<uint8_t>* plaintext);
  bool Decrypt(const std::string& ciphertext, std::string* plaintext);
  bool Decrypt(const uint8_t* ciphertext,
               size_t ciphertext_size,
               uint8_t* plaintext) {
    size_t plaintext_size;
    return DecryptInternal(ciphertext, ciphertext_size, plaintext,
                           &plaintext_size);
  }
  /// @}

  /// Set IV.
  /// @return true if successful, false if the input is invalid.
  virtual bool SetIv(const std::vector<uint8_t>& iv) = 0;

 protected:
  /// Internal implementation of decrypt function.
  /// @param ciphertext points to the input ciphertext.
  /// @param ciphertext_size is the input ciphertext size.
  /// @param[out] plaintext points to the output plaintext. @a plaintext and
  ///             @a ciphertext can point to the same address.
  /// @param[out] plaintext_size contains the size of plaintext on success.
  ///             It should never be larger than @a ciphertext_size.
  virtual bool DecryptInternal(const uint8_t* ciphertext,
                               size_t ciphertext_size,
                               uint8_t* plaintext,
                               size_t* plaintext_size) = 0;

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

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

  uint32_t block_offset() const { return encryptor_->block_offset(); }

 protected:
  bool DecryptInternal(const uint8_t* ciphertext,
                       size_t ciphertext_size,
                       uint8_t* plaintext,
                       size_t* plaintext_size) override;

 private:
  scoped_ptr<AesCtrEncryptor> encryptor_;

  DISALLOW_COPY_AND_ASSIGN(AesCtrDecryptor);
};

// Class which implements AES-CBC (Cipher block chaining) decryption.
class AesCbcDecryptor : public AesDecryptor {
 public:
  /// @param padding_scheme indicates the padding scheme used. Currently
  ///        supported schemes: kNoPadding, kPkcs5Padding, kCtsPadding.
  /// @param chain_across_calls indicates whether there is a continuous cipher
  ///        block chain across calls for Decrypt function. If it is false, iv
  ///        is not updated across Decrypt function calls.
  AesCbcDecryptor(CbcPaddingScheme padding_scheme, bool chain_across_calls);
  ~AesCbcDecryptor() override;

  /// @name AesDecryptor implementation overrides.
  /// @{
  bool InitializeWithIv(const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& iv) override;

  bool SetIv(const std::vector<uint8_t>& iv) override;
  /// @}

 protected:
  bool DecryptInternal(const uint8_t* ciphertext,
                       size_t ciphertext_size,
                       uint8_t* plaintext,
                       size_t* plaintext_size) override;

 private:
  // Openssl AES_KEY.
  scoped_ptr<AES_KEY> aes_key_;
  // Initialization vector, must be 16 for CBC.
  std::vector<uint8_t> iv_;
  const CbcPaddingScheme padding_scheme_;
  const bool chain_across_calls_;

  DISALLOW_COPY_AND_ASSIGN(AesCbcDecryptor);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_AES_DECRYPTOR_H_
