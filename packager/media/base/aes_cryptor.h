// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_AES_CRYPTOR_H_
#define PACKAGER_MEDIA_BASE_AES_CRYPTOR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <mbedtls/cipher.h>

#include <packager/macros/classes.h>
#include <packager/media/base/fourccs.h>

namespace shaka {
namespace media {

// AES cryptor interface. Inherited by various AES encryptor and decryptor
// implementations.
class AesCryptor {
 public:
  enum ConstantIvFlag {
    kUseConstantIv,
    kDontUseConstantIv,
  };

  /// @param constant_iv_flag indicates whether a constant iv is used,
  ///        kUseConstantIv means that the same iv is used for all Crypt calls
  ///        until iv is changed via SetIv; otherwise, iv can be incremented
  ///        (for counter mode) or chained (for cipher block chaining mode)
  ///        internally inside Crypt call, i.e. iv will be updated across Crypt
  ///        calls.
  explicit AesCryptor(ConstantIvFlag constant_iv_flag);
  virtual ~AesCryptor();

  /// Initialize the cryptor with specified key and IV.
  /// @return true on successful initialization, false otherwise.
  virtual bool InitializeWithIv(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& iv) = 0;

  virtual size_t RequiredOutputSize(size_t plaintext_size) {
    return plaintext_size;
  }

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
    return Crypt(text, text_size, crypt_text, &crypt_text_size);
  }
  bool Crypt(const uint8_t* text,
             size_t text_size,
             uint8_t* crypt_text,
             size_t* crypt_text_size) {
    if (constant_iv_flag_ == kUseConstantIv)
      SetIvInternal();
    else
      num_crypt_bytes_ += text_size;
    return CryptInternal(text, text_size, crypt_text, crypt_text_size);
  }
  /// @}

  /// Set IV. SetIv() implementation guarantees that the iv passed to SetIv()
  /// is set to iv() and then calls SetIvInternal().
  /// @return true if successful, false if the input is invalid.
  bool SetIv(const std::vector<uint8_t>& iv);

  /// Update IV for next sample. As recommended in ISO/IEC 23001-7: IV need to
  /// be updated per sample for CENC.
  /// This is used by encryptors only. It is a NOP if using kUseConstantIv.
  void UpdateIv();

  /// @return The current iv.
  const std::vector<uint8_t>& iv() const { return iv_; }

  /// @return true if constant iv is used, false otherwise.
  bool use_constant_iv() const { return constant_iv_flag_ == kUseConstantIv; }

  /// @param protection_scheme specifies the protection scheme: 'cenc', 'cens',
  ///        'cbc1', 'cbcs', which is useful to determine the random iv size.
  /// @param iv points to generated initialization vector.
  /// @return true on success, false otherwise.
  static bool GenerateRandomIv(FourCC protection_scheme,
                               std::vector<uint8_t>* iv);

 protected:
  enum CipherMode {
    kCtrMode,
    kCbcMode,
  };

  // mbedTLS cipher context.
  mbedtls_cipher_context_t cipher_ctx_;

  bool SetupCipher(size_t key_size, CipherMode mode);

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

  // Internal implementation of SetIv, which setup internal iv.
  virtual void SetIvInternal() = 0;

  // |size| specifies the input text size.
  // Return the number of padding bytes needed.
  // Note: No paddings should be needed except for pkcs5-cbc encryptor.
  virtual size_t NumPaddingBytes(size_t size) const;

  // Indicates whether a constant iv is used. Internal iv will be reset to
  // |iv_| before calling Crypt if that is the case.
  const ConstantIvFlag constant_iv_flag_;
  // Initialization vector from by SetIv or InitializeWithIv, with size 8 or 16
  // bytes.
  std::vector<uint8_t> iv_;
  // Tracks number of crypt bytes. It is used to calculate how many blocks
  // should iv advance in UpdateIv(). It will be reset to 0 after iv is updated.
  size_t num_crypt_bytes_;

  DISALLOW_COPY_AND_ASSIGN(AesCryptor);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_AES_CRYPTOR_H_
