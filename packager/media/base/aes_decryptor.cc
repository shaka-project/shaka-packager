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
  // mbedtls requires a buffer large enough for one extra block.
  return plaintext_size + AES_BLOCK_SIZE;
}

bool AesCbcDecryptor::CryptInternal(const uint8_t* ciphertext,
                                    size_t ciphertext_size,
                                    uint8_t* plaintext,
                                    size_t* plaintext_size) {
  DCHECK(plaintext_size);
  // Plaintext size is the same as ciphertext size except for pkcs5 padding.
  // Will update later if using pkcs5 padding. For pkcs5 padding, we still
  // need at least |ciphertext_size| bytes for intermediate operation.
  // mbedtls requires a buffer large enough for one extra block.
  const size_t required_plaintext_size = ciphertext_size + AES_BLOCK_SIZE;
  if (*plaintext_size < required_plaintext_size) {
    LOG(ERROR) << "Expecting output size of at least "
               << required_plaintext_size << " bytes.";
    return false;
  }
  *plaintext_size = required_plaintext_size - AES_BLOCK_SIZE;

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

  // Copy the residual block early, since mbedtls may overwrite one extra block
  // of the output, and input and output may be the same buffer.
  std::vector<uint8_t> residual_block(ciphertext + cbc_size,
                                      ciphertext + ciphertext_size);
  DCHECK_EQ(residual_block.size(), residual_block_size);

  if (residual_block_size == 0) {
    CbcDecryptBlocks(ciphertext, ciphertext_size, plaintext);
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
    CbcDecryptBlocks(ciphertext, cbc_size, plaintext);

    // The residual block is not encrypted.
    memcpy(plaintext + cbc_size, residual_block.data(), residual_block_size);
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

  // Copy the next-to-last block early, since mbedtls may overwrite one extra
  // block of the output, and input and output may be the same buffer.
  // NOTE: Before this point, there may not be such a block.  Here, we know
  // this is safe.
  std::vector<uint8_t> next_to_last_block(
      ciphertext + cbc_size - AES_BLOCK_SIZE, ciphertext + cbc_size);

  // AES-CBC decrypt everything up to the next-to-last full block.
  if (cbc_size > AES_BLOCK_SIZE) {
    CbcDecryptBlocks(ciphertext, cbc_size - AES_BLOCK_SIZE, plaintext);
  }

  uint8_t* next_to_last_plaintext_block = plaintext + cbc_size - AES_BLOCK_SIZE;

  // The next-to-last block should be decrypted first in ECB mode, which is
  // effectively what you get with an IV of all zeroes.
  std::vector<uint8_t> backup_iv(internal_iv_);
  internal_iv_.assign(AES_BLOCK_SIZE, 0);
  // mbedtls requires a buffer large enough for one extra block.
  std::vector<uint8_t> stolen_bits(AES_BLOCK_SIZE * 2);
  CbcDecryptBlocks(next_to_last_block.data(), AES_BLOCK_SIZE,
                   stolen_bits.data());

  // Reconstruct the final two blocks of ciphertext.
  std::vector<uint8_t> reconstructed_blocks(AES_BLOCK_SIZE * 2);
  memcpy(reconstructed_blocks.data(), residual_block.data(),
         residual_block_size);
  memcpy(reconstructed_blocks.data() + residual_block_size,
         stolen_bits.data() + residual_block_size,
         AES_BLOCK_SIZE - residual_block_size);
  memcpy(reconstructed_blocks.data() + AES_BLOCK_SIZE,
         next_to_last_block.data(), AES_BLOCK_SIZE);

  // Decrypt the last two blocks.
  internal_iv_ = backup_iv;
  // mbedtls requires a buffer large enough for one extra block.
  std::vector<uint8_t> final_output_blocks(AES_BLOCK_SIZE * 3);
  CbcDecryptBlocks(reconstructed_blocks.data(), AES_BLOCK_SIZE * 2,
                   final_output_blocks.data());

  // Copy the final output.
  memcpy(next_to_last_plaintext_block, final_output_blocks.data(),
         AES_BLOCK_SIZE + residual_block_size);
  return true;
}

void AesCbcDecryptor::SetIvInternal() {
  internal_iv_ = iv();
  internal_iv_.resize(AES_BLOCK_SIZE, 0);
}

void AesCbcDecryptor::CbcDecryptBlocks(const uint8_t* ciphertext,
                                       size_t ciphertext_size,
                                       uint8_t* plaintext) {
  CHECK_EQ(ciphertext_size % AES_BLOCK_SIZE, 0u);
  CHECK_GT(ciphertext_size, 0u);

  // Copy the final block of ciphertext before decryption, since we could be
  // decrypting in-place.
  const uint8_t* last_block = ciphertext + ciphertext_size - AES_BLOCK_SIZE;
  std::vector<uint8_t> next_iv(last_block, last_block + AES_BLOCK_SIZE);

  size_t output_size = 0;
  CHECK_EQ(mbedtls_cipher_crypt(&cipher_ctx_, internal_iv_.data(),
                                AES_BLOCK_SIZE, ciphertext, ciphertext_size,
                                plaintext, &output_size),
           0);
  DCHECK_EQ(output_size % AES_BLOCK_SIZE, 0u);

  // Update the internal IV.
  internal_iv_ = next_iv;
}

}  // namespace media
}  // namespace shaka
