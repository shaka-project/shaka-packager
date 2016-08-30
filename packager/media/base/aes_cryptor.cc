// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/aes_cryptor.h"

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <string>
#include <vector>

#include "packager/base/logging.h"

namespace {

// According to ISO/IEC 23001-7:2016 CENC spec, IV should be either
// 64-bit (8-byte) or 128-bit (16-byte).
bool IsIvSizeValid(size_t iv_size) {
  return iv_size == 8 || iv_size == 16;
}

}  // namespace

namespace shaka {
namespace media {

AesCryptor::AesCryptor(ConstantIvFlag constant_iv_flag)
    : aes_key_(new AES_KEY),
      constant_iv_flag_(constant_iv_flag),
      num_crypt_bytes_(0) {}

AesCryptor::~AesCryptor() {}

bool AesCryptor::Crypt(const std::vector<uint8_t>& text,
                       std::vector<uint8_t>* crypt_text) {
  // Save text size to make it work for in-place conversion, since the
  // next statement will update the text size.
  const size_t text_size = text.size();
  crypt_text->resize(text_size + NumPaddingBytes(text_size));
  size_t crypt_text_size = crypt_text->size();
  if (!Crypt(text.data(), text_size, crypt_text->data(), &crypt_text_size)) {
    return false;
  }
  DCHECK_LE(crypt_text_size, crypt_text->size());
  crypt_text->resize(crypt_text_size);
  return true;
}

bool AesCryptor::Crypt(const std::string& text, std::string* crypt_text) {
  // Save text size to make it work for in-place conversion, since the
  // next statement will update the text size.
  const size_t text_size = text.size();
  crypt_text->resize(text_size + NumPaddingBytes(text_size));
  size_t crypt_text_size = crypt_text->size();
  if (!Crypt(reinterpret_cast<const uint8_t*>(text.data()), text_size,
             reinterpret_cast<uint8_t*>(&(*crypt_text)[0]), &crypt_text_size))
    return false;
  DCHECK_LE(crypt_text_size, crypt_text->size());
  crypt_text->resize(crypt_text_size);
  return true;
}

bool AesCryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (!IsIvSizeValid(iv.size())) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }
  iv_ = iv;
  num_crypt_bytes_ = 0;
  SetIvInternal();
  return true;
}

void AesCryptor::UpdateIv() {
  if (constant_iv_flag_ == kUseConstantIv)
    return;

  uint64_t increment = 0;
  // As recommended in ISO/IEC 23001-7:2016 CENC spec, for 64-bit (8-byte)
  // IV_Sizes, initialization vectors for subsequent samples can be created by
  // incrementing the initialization vector of the previous sample.
  // For 128-bit (16-byte) IV_Sizes, initialization vectors for subsequent
  // samples should be created by adding the block count of the previous sample
  // to the initialization vector of the previous sample.
  // There is no official recommendation of how IV for next sample should be
  // generated for CBC mode. We use the same generation algorithm as CTR here.
  if (iv_.size() == 8) {
    increment = 1;
  } else {
    DCHECK_EQ(16u, iv_.size());
    increment = (num_crypt_bytes_ + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE;
  }

  for (int i = iv_.size() - 1; increment > 0 && i >= 0; --i) {
    increment += iv_[i];
    iv_[i] = increment & 0xFF;
    increment >>= 8;
  }
  num_crypt_bytes_ = 0;
  SetIvInternal();
}

bool AesCryptor::GenerateRandomIv(FourCC protection_scheme,
                                  std::vector<uint8_t>* iv) {
  // ISO/IEC 23001-7:2016 10.1 and 10.3 For 'cenc' and 'cens'
  // default_Per_Sample_IV_Size and Per_Sample_IV_Size SHOULD be 8-bytes.
  // There is no official guideline on the iv size for 'cbc1' and 'cbcs',
  // but 16-byte provides better security.
  const size_t iv_size =
      (protection_scheme == FOURCC_cenc || protection_scheme == FOURCC_cens)
          ? 8
          : 16;
  iv->resize(iv_size);
  if (RAND_bytes(iv->data(), iv_size) != 1) {
    LOG(ERROR) << "RAND_bytes failed with error: "
               << ERR_error_string(ERR_get_error(), NULL);
    return false;
  }
  return true;
}

size_t AesCryptor::NumPaddingBytes(size_t size) const {
  // No padding by default.
  return 0;
}

}  // namespace media
}  // namespace shaka
