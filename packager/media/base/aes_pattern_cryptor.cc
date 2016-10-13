// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/aes_pattern_cryptor.h"

#include <openssl/aes.h>
#include <algorithm>
#include "packager/base/logging.h"

namespace shaka {
namespace media {

AesPatternCryptor::AesPatternCryptor(uint8_t crypt_byte_block,
                                     uint8_t skip_byte_block,
                                     PatternEncryptionMode encryption_mode,
                                     ConstantIvFlag constant_iv_flag,
                                     std::unique_ptr<AesCryptor> cryptor)
    : AesCryptor(constant_iv_flag),
      crypt_byte_block_(crypt_byte_block),
      skip_byte_block_(skip_byte_block),
      encryption_mode_(encryption_mode),
      cryptor_(std::move(cryptor)) {
  // Treat pattern 0:0 as 1:0.
  if (crypt_byte_block_ == 0 && skip_byte_block_ == 0)
    crypt_byte_block_ = 1;
  DCHECK(cryptor_);
  DCHECK(!cryptor_->use_constant_iv());
}

AesPatternCryptor::~AesPatternCryptor() {}

bool AesPatternCryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                         const std::vector<uint8_t>& iv) {
  return SetIv(iv) && cryptor_->InitializeWithIv(key, iv);
}

bool AesPatternCryptor::CryptInternal(const uint8_t* text,
                                      size_t text_size,
                                      uint8_t* crypt_text,
                                      size_t* crypt_text_size) {
  // |crypt_text_size| is always the same as |text_size| for pattern encryption.
  if (*crypt_text_size < text_size) {
    LOG(ERROR) << "Expecting output size of at least " << text_size
               << " bytes.";
    return false;
  }
  *crypt_text_size = text_size;

  while (text_size > 0) {
    const size_t crypt_byte_size = crypt_byte_block_ * AES_BLOCK_SIZE;
    if (NeedEncrypt(text_size, crypt_byte_size)) {
      if (!cryptor_->Crypt(text, crypt_byte_size, crypt_text))
        return false;
    } else {
      // If there is not enough data, just keep it in clear.
      memcpy(crypt_text, text, text_size);
      return true;
    }
    text += crypt_byte_size;
    text_size -= crypt_byte_size;
    crypt_text += crypt_byte_size;

    const size_t skip_byte_size = std::min(
        static_cast<size_t>(skip_byte_block_ * AES_BLOCK_SIZE), text_size);
    memcpy(crypt_text, text, skip_byte_size);
    text += skip_byte_size;
    text_size -= skip_byte_size;
    crypt_text += skip_byte_size;
  }
  return true;
}

void AesPatternCryptor::SetIvInternal() {
  CHECK(cryptor_->SetIv(iv()));
}

bool AesPatternCryptor::NeedEncrypt(size_t input_size,
                                    size_t target_data_size) {
  if (encryption_mode_ == kSkipIfCryptByteBlockRemaining)
    return input_size > target_data_size;
  return input_size >= target_data_size;
}

}  // namespace media
}  // namespace shaka
