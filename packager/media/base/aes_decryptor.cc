// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/aes_decryptor.h"

#include <openssl/aes.h>
#include <algorithm>
#include "packager/base/logging.h"

namespace {

// AES defines three key sizes: 128, 192 and 256 bits.
bool IsKeySizeValidForAes(size_t key_size) {
  return key_size == 16 || key_size == 24 || key_size == 32;
}

}  // namespace

namespace shaka {
namespace media {

AesCbcDecryptor::AesCbcDecryptor(CbcPaddingScheme padding_scheme)
    : AesCbcDecryptor(padding_scheme, kDontUseConstantIv) {}

AesCbcDecryptor::AesCbcDecryptor(CbcPaddingScheme padding_scheme,
                                 ConstantIvFlag constant_iv_flag)
    : AesCryptor(constant_iv_flag), padding_scheme_(padding_scheme) {
  if (padding_scheme_ != kNoPadding) {
    CHECK_EQ(constant_iv_flag, kUseConstantIv)
        << "non-constant iv (cipher block chain across calls) only makes sense "
           "if the padding_scheme is kNoPadding.";
  }
}

AesCbcDecryptor::~AesCbcDecryptor() {}

bool AesCbcDecryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& iv) {
  if (!IsKeySizeValidForAes(key.size())) {
    LOG(ERROR) << "Invalid AES key size: " << key.size();
    return false;
  }

  CHECK_EQ(AES_set_decrypt_key(key.data(), key.size() * 8, mutable_aes_key()),
           0);
  return SetIv(iv);
}

bool AesCbcDecryptor::CryptInternal(const uint8_t* ciphertext,
                                    size_t ciphertext_size,
                                    uint8_t* plaintext,
                                    size_t* plaintext_size) {
  DCHECK(plaintext_size);
  DCHECK(aes_key());
  // Plaintext size is the same as ciphertext size except for pkcs5 padding.
  // Will update later if using pkcs5 padding. For pkcs5 padding, we still
  // need at least |ciphertext_size| bytes for intermediate operation.
  if (*plaintext_size < ciphertext_size) {
    LOG(ERROR) << "Expecting output size of at least " << ciphertext_size
               << " bytes.";
    return false;
  }
  *plaintext_size = ciphertext_size;

  if (ciphertext_size == 0) {
    if (padding_scheme_ == kPkcs5Padding) {
      LOG(ERROR) << "Expected ciphertext to be at least " << AES_BLOCK_SIZE
                 << " bytes with Pkcs5 padding.";
      return false;
    }
    return true;
  }
  DCHECK(plaintext);

  const size_t residual_block_size = ciphertext_size % AES_BLOCK_SIZE;
  const size_t cbc_size = ciphertext_size - residual_block_size;
  if (residual_block_size == 0) {
    AES_cbc_encrypt(ciphertext, plaintext, ciphertext_size, aes_key(),
                    internal_iv_.data(), AES_DECRYPT);
    if (padding_scheme_ != kPkcs5Padding)
      return true;

    // Strip off PKCS5 padding bytes.
    const uint8_t num_padding_bytes = plaintext[ciphertext_size - 1];
    if (num_padding_bytes > AES_BLOCK_SIZE) {
      LOG(ERROR) << "Padding length is too large : "
                 << static_cast<int>(num_padding_bytes);
      return false;
    }
    *plaintext_size -= num_padding_bytes;
    return true;
  } else if (padding_scheme_ == kNoPadding) {
    AES_cbc_encrypt(ciphertext, plaintext, cbc_size, aes_key(),
                    internal_iv_.data(), AES_DECRYPT);

    // The residual block is not encrypted.
    memcpy(plaintext + cbc_size, ciphertext + cbc_size, residual_block_size);
    return true;
  } else if (padding_scheme_ != kCtsPadding) {
    LOG(ERROR) << "Expecting cipher text size to be multiple of "
               << AES_BLOCK_SIZE << ", got " << ciphertext_size;
    return false;
  }

  DCHECK_EQ(padding_scheme_, kCtsPadding);
  if (ciphertext_size < AES_BLOCK_SIZE) {
    // Don't have a full block, leave unencrypted.
    memcpy(plaintext, ciphertext, ciphertext_size);
    return true;
  }

  // AES-CBC decrypt everything up to the next-to-last full block.
  if (cbc_size > AES_BLOCK_SIZE) {
    AES_cbc_encrypt(ciphertext, plaintext, cbc_size - AES_BLOCK_SIZE, aes_key(),
                    internal_iv_.data(), AES_DECRYPT);
  }

  const uint8_t* next_to_last_ciphertext_block =
      ciphertext + ciphertext_size - residual_block_size - AES_BLOCK_SIZE;
  uint8_t* next_to_last_plaintext_block =
      plaintext + ciphertext_size - residual_block_size - AES_BLOCK_SIZE;

  // Determine what the last IV should be so that we can "skip ahead" in the
  // CBC decryption.
  std::vector<uint8_t> last_iv(
      ciphertext + ciphertext_size - residual_block_size,
      ciphertext + ciphertext_size);
  last_iv.resize(AES_BLOCK_SIZE, 0);

  // Decrypt the next-to-last block using the IV determined above. This decrypts
  // the residual block bits.
  AES_cbc_encrypt(next_to_last_ciphertext_block, next_to_last_plaintext_block,
                  AES_BLOCK_SIZE, aes_key(), last_iv.data(), AES_DECRYPT);

  // Swap back the residual block bits and the next-to-last block.
  if (plaintext == ciphertext) {
    std::swap_ranges(next_to_last_plaintext_block,
                     next_to_last_plaintext_block + residual_block_size,
                     next_to_last_plaintext_block + AES_BLOCK_SIZE);
  } else {
    memcpy(next_to_last_plaintext_block + AES_BLOCK_SIZE,
           next_to_last_plaintext_block, residual_block_size);
    memcpy(next_to_last_plaintext_block,
           next_to_last_ciphertext_block + AES_BLOCK_SIZE, residual_block_size);
  }

  // Decrypt the next-to-last full block.
  AES_cbc_encrypt(next_to_last_plaintext_block, next_to_last_plaintext_block,
                  AES_BLOCK_SIZE, aes_key(), internal_iv_.data(), AES_DECRYPT);
  return true;
}

void AesCbcDecryptor::SetIvInternal() {
  internal_iv_ = iv();
  internal_iv_.resize(AES_BLOCK_SIZE, 0);
}

}  // namespace media
}  // namespace shaka
