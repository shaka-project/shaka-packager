// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/aes_decryptor.h>

#include <algorithm>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/crypto.h>

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
  if (!SetupCipher(key.size(), kCbcMode)) {
    return false;
  }

  if (mbedtls_cipher_setkey(&cipher_ctx_, key.data(),
                            static_cast<int>(8 * key.size()),
                            MBEDTLS_DECRYPT) != 0) {
    LOG(ERROR) << "Failed to set CBC decryption key";
    return false;
  }

  return SetIv(iv);
}

size_t AesCbcDecryptor::RequiredOutputSize(size_t plaintext_size) {
  return plaintext_size;
}

bool AesCbcDecryptor::CryptInternal(const uint8_t* ciphertext,
                                    size_t ciphertext_size,
                                    uint8_t* plaintext,
                                    size_t* plaintext_size) {
  DCHECK(plaintext_size);
  // Plaintext size is the same as ciphertext size except for pkcs5 padding.
  // Will update later if using pkcs5 padding. For pkcs5 padding, we still
  // need at least |ciphertext_size| bytes for intermediate operation.
  if (*plaintext_size < ciphertext_size) {
    LOG(ERROR) << "Expecting output size of at least " << ciphertext_size
               << " bytes.";
    return false;
  }
  *plaintext_size = ciphertext_size;

  // If the ciphertext size is 0, this can be a no-op decrypt, so long as the
  // padding mode isn't PKCS5.
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
    CbcDecryptBlocks(ciphertext, ciphertext_size, plaintext,
                     internal_iv_.data());
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
    if (cbc_size > 0) {
      CbcDecryptBlocks(ciphertext, cbc_size, plaintext, internal_iv_.data());
    }
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
    CbcDecryptBlocks(ciphertext, cbc_size - AES_BLOCK_SIZE, plaintext,
                     internal_iv_.data());
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
  CbcDecryptBlocks(next_to_last_ciphertext_block, AES_BLOCK_SIZE,
                   next_to_last_plaintext_block, last_iv.data());

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
  CbcDecryptBlocks(next_to_last_plaintext_block, AES_BLOCK_SIZE,
                   next_to_last_plaintext_block, internal_iv_.data());
  return true;
}

void AesCbcDecryptor::SetIvInternal() {
  internal_iv_ = iv();
  internal_iv_.resize(AES_BLOCK_SIZE, 0);
}

void AesCbcDecryptor::CbcDecryptBlocks(const uint8_t* ciphertext,
                                       size_t ciphertext_size,
                                       uint8_t* plaintext,
                                       uint8_t* iv) {
  CHECK_EQ(ciphertext_size % AES_BLOCK_SIZE, 0u);
  CHECK_GT(ciphertext_size, 0u);

  // Copy the final block of ciphertext before decryption, since we could be
  // decrypting in-place.
  const uint8_t* last_block = ciphertext + ciphertext_size - AES_BLOCK_SIZE;
  std::vector<uint8_t> next_iv(last_block, last_block + AES_BLOCK_SIZE);

  size_t output_size = 0;
  CHECK_EQ(mbedtls_cipher_crypt(&cipher_ctx_, iv, AES_BLOCK_SIZE, ciphertext,
                                ciphertext_size, plaintext, &output_size),
           0);
  DCHECK_EQ(output_size % AES_BLOCK_SIZE, 0u);

  memcpy(iv, next_iv.data(), next_iv.size());
}

}  // namespace media
}  // namespace shaka
