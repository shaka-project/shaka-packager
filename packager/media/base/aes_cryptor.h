// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_AES_CRYPTOR_H_
#define PACKAGER_MEDIA_BASE_AES_CRYPTOR_H_

#include <string>
#include <vector>

#include "packager/base/macros.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/fourccs.h"

struct aes_key_st;
typedef struct aes_key_st AES_KEY;

namespace edash_packager {
namespace media {

// AES cryptor interface. Inherited by various AES encryptor and decryptor
// implementations.
class AesCryptor {
 public:
  AesCryptor();
  virtual ~AesCryptor();

  /// Initialize the cryptor with specified key and IV.
  /// @return true on successful initialization, false otherwise.
  virtual bool InitializeWithIv(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& iv) = 0;

  /// @name Various forms of crypt (Encrypt/Decrypt) calls.
  /// It is an Encrypt function for encryptor and a Decrypt function for
  /// decryptor. The text and crypt_text pointers can be the same address for
  /// in place encryption/decryption.
  /// @{
  bool Crypt(const std::vector<uint8_t>& text,
             std::vector<uint8_t>* crypt_text);
  bool Crypt(const std::string& text, std::string* crypt_text);
  /// @param crypt_text should have at least @a text_size bytes.
  bool Crypt(const uint8_t* text, size_t text_size, uint8_t* crypt_text) {
    size_t crypt_text_size = text_size;
    return CryptInternal(text, text_size, crypt_text, &crypt_text_size);
  }
  /// @}

  /// Set IV.
  /// @return true if successful, false if the input is invalid.
  virtual bool SetIv(const std::vector<uint8_t>& iv) = 0;

  /// Update IV for next sample. As recommended in ISO/IEC 23001-7: IV need to
  /// be updated per sample for CENC.
  /// This is used by encryptors only.
  virtual void UpdateIv() = 0;

  /// @return The current iv.
  const std::vector<uint8_t>& iv() const { return iv_; }

  /// @param protection_scheme specifies the protection scheme: 'cenc', 'cens',
  ///        'cbc1', 'cbcs', which is useful to determine the random iv size.
  /// @param iv points to generated initialization vector.
  /// @return true on success, false otherwise.
  static bool GenerateRandomIv(FourCC protection_scheme,
                               std::vector<uint8_t>* iv);

 protected:
  void set_iv(const std::vector<uint8_t>& iv) { iv_ = iv; }
  const AES_KEY* aes_key() const { return aes_key_.get(); }
  AES_KEY* mutable_aes_key() { return aes_key_.get(); }

 private:
  // Internal implementation of crypt function.
  // |text| points to the input text.
  // |text_size| is the size of input text.
  // |crypt_text| points to the output encrypted or decrypted text, depends on
  // whether it is an encryption or decryption. |text| and |crypt_text| can
  // point to the same address for in place encryption/decryption.
  // |crypt_text_size| contains the size of |crypt_text| and it will be updated
  // to contain the actual encrypted/decrypted size for |crypt_text| on success.
  // Return false if the input |crypt_text_size| is not large enough to hold the
  // output |crypt_text| or if there is any error in encryption/decryption.
  virtual bool CryptInternal(const uint8_t* text,
                             size_t text_size,
                             uint8_t* crypt_text,
                             size_t* crypt_text_size) = 0;

  // |size| specifies the input text size.
  // Return the number of padding bytes needed.
  // Note: No paddings should be needed except for pkcs5-cbc encryptor.
  virtual size_t NumPaddingBytes(size_t size) const;

  // Initialization vector, with size 8 or 16.
  std::vector<uint8_t> iv_;
  // Openssl AES_KEY.
  scoped_ptr<AES_KEY> aes_key_;

  DISALLOW_COPY_AND_ASSIGN(AesCryptor);
};

}  // namespace media
}  // namespace edash_packager

#endif  // PACKAGER_MEDIA_BASE_AES_CRYPTOR_H_


