// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/aes_encryptor.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/crypto.h>

namespace {

// Increment an 8-byte counter by 1. Return true if overflowed.
bool Increment64(uint8_t* counter) {
  DCHECK(counter);
  for (int i = 7; i >= 0; --i) {
    if (++counter[i] != 0)
      return false;
  }
  return true;
}

}  // namespace

namespace shaka {
namespace media {

// We don't support constant iv for counter mode, as we don't have a use case
// for that.
AesCtrEncryptor::AesCtrEncryptor()
    : AesCryptor(kDontUseConstantIv),
      block_offset_(0),
      // mbedtls requires an extra output block.
      encrypted_counter_(AES_BLOCK_SIZE * 2, 0) {}

AesCtrEncryptor::~AesCtrEncryptor() {}

bool AesCtrEncryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& iv) {
  if (!SetupCipher(key.size(), kCtrMode)) {
    return false;
  }

  if (mbedtls_cipher_setkey(&cipher_ctx_, key.data(),
                            static_cast<int>(8 * key.size()),
                            MBEDTLS_ENCRYPT) != 0) {
    LOG(ERROR) << "Failed to set CTR encryption key";
    return false;
  }

  return SetIv(iv);
}

bool AesCtrEncryptor::CryptInternal(const uint8_t* plaintext,
                                    size_t plaintext_size,
                                    uint8_t* ciphertext,
                                    size_t* ciphertext_size) {
  DCHECK(plaintext);
  DCHECK(ciphertext);

  // |ciphertext_size| is always the same as |plaintext_size| for counter mode.
  if (*ciphertext_size < plaintext_size) {
    LOG(ERROR) << "Expecting output size of at least " << plaintext_size
               << " bytes.";
    return false;
  }
  *ciphertext_size = plaintext_size;

  for (size_t i = 0; i < plaintext_size; ++i) {
    if (block_offset_ == 0) {
      size_t ignored_output_size;
      CHECK_EQ(
          mbedtls_cipher_crypt(&cipher_ctx_, /* iv= */ NULL, /* iv_len= */ 0,
                               &counter_[0], AES_BLOCK_SIZE,
                               &encrypted_counter_[0], &ignored_output_size),
          0);

      // As mentioned in ISO/IEC 23001-7:2016 CENC spec, of the 16 byte counter
      // block, bytes 8 to 15 (i.e. the least significant bytes) are used as a
      // simple 64 bit unsigned integer that is incremented by one for each
      // subsequent block of sample data processed and is kept in network byte
      // order.
      Increment64(&counter_[8]);
    }
    ciphertext[i] = plaintext[i] ^ encrypted_counter_[block_offset_];
    block_offset_ = (block_offset_ + 1) % AES_BLOCK_SIZE;
  }
  return true;
}

void AesCtrEncryptor::SetIvInternal() {
  block_offset_ = 0;
  counter_ = iv();
  counter_.resize(AES_BLOCK_SIZE, 0);
}

AesCbcEncryptor::AesCbcEncryptor(CbcPaddingScheme padding_scheme)
    : AesCbcEncryptor(padding_scheme, kDontUseConstantIv) {}

AesCbcEncryptor::AesCbcEncryptor(CbcPaddingScheme padding_scheme,
                                 ConstantIvFlag constant_iv_flag)
    : AesCryptor(constant_iv_flag), padding_scheme_(padding_scheme) {
  if (padding_scheme_ != kNoPadding) {
    CHECK_EQ(constant_iv_flag, kUseConstantIv)
        << "non-constant iv (cipher block chain across calls) only makes sense "
           "if the padding_scheme is kNoPadding.";
  }
}

AesCbcEncryptor::~AesCbcEncryptor() {}

bool AesCbcEncryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& iv) {
  if (!SetupCipher(key.size(), kCbcMode)) {
    return false;
  }

  if (mbedtls_cipher_setkey(&cipher_ctx_, key.data(),
                            static_cast<int>(8 * key.size()),
                            MBEDTLS_ENCRYPT) != 0) {
    LOG(ERROR) << "Failed to set CBC encryption key";
    return false;
  }

  return SetIv(iv);
}

size_t AesCbcEncryptor::RequiredOutputSize(size_t plaintext_size) {
  // mbedtls requires a buffer large enough for one extra block.
  return plaintext_size + NumPaddingBytes(plaintext_size) + AES_BLOCK_SIZE;
}

bool AesCbcEncryptor::CryptInternal(const uint8_t* plaintext,
                                    size_t plaintext_size,
                                    uint8_t* ciphertext,
                                    size_t* ciphertext_size) {
  const size_t residual_block_size = plaintext_size % AES_BLOCK_SIZE;
  const size_t num_padding_bytes = NumPaddingBytes(plaintext_size);
  const size_t required_ciphertext_size = RequiredOutputSize(plaintext_size);

  if (*ciphertext_size < required_ciphertext_size) {
    LOG(ERROR) << "Expecting output size of at least "
               << required_ciphertext_size << " bytes.";
    return false;
  }
  *ciphertext_size = required_ciphertext_size - AES_BLOCK_SIZE;

  // Encrypt everything but the residual block using CBC.
  const size_t cbc_size = plaintext_size - residual_block_size;

  // Copy the residual block early, since mbedtls may overwrite one extra block
  // of the output, and input and output may be the same buffer.
  std::vector<uint8_t> residual_block(plaintext + cbc_size,
                                      plaintext + plaintext_size);
  DCHECK_EQ(residual_block.size(), residual_block_size);

  if (cbc_size != 0) {
    CbcEncryptBlocks(plaintext, cbc_size, ciphertext);
  } else if (padding_scheme_ == kCtsPadding) {
    // Don't have a full block, leave unencrypted.
    memcpy(ciphertext, plaintext, plaintext_size);
    return true;
  }
  if (residual_block_size == 0 && padding_scheme_ != kPkcs5Padding) {
    // No residual block. No need to do padding.
    return true;
  }

  if (padding_scheme_ == kNoPadding) {
    // The residual block is left unencrypted.
    memcpy(ciphertext + cbc_size, plaintext + cbc_size, residual_block_size);
    return true;
  }

  uint8_t* residual_ciphertext_block = ciphertext + cbc_size;
  if (padding_scheme_ == kPkcs5Padding) {
    DCHECK_EQ(num_padding_bytes, AES_BLOCK_SIZE - residual_block_size);

    // Pad residue block with PKCS5 padding.
    residual_block.resize(AES_BLOCK_SIZE, static_cast<char>(num_padding_bytes));

    CbcEncryptBlocks(residual_block.data(), AES_BLOCK_SIZE,
                     residual_ciphertext_block);
  } else {
    DCHECK_EQ(num_padding_bytes, 0u);
    DCHECK_EQ(padding_scheme_, kCtsPadding);

    // Zero-pad the residual block and encrypt using CBC.
    residual_block.resize(AES_BLOCK_SIZE, 0);
    // mbedtls requires an extra block in the output buffer, and it cannot be
    // the same as the input buffer.
    std::vector<uint8_t> encrypted_residual_block(AES_BLOCK_SIZE * 2);

    CbcEncryptBlocks(residual_block.data(), AES_BLOCK_SIZE,
                     encrypted_residual_block.data());

    // Replace the last full block with the zero-padded, encrypted residual
    // block, and replace the residual block with the equivalent portion of the
    // last full encrypted block. It may appear that some encrypted bits of the
    // last full block are lost, but they are not, as they were used as the IV
    // when encrypting the zero-padded residual block.
    // This ordering of the output is described as "CS2" in literature.
    // https://en.wikipedia.org/wiki/Ciphertext_stealing#CS2
    memcpy(residual_ciphertext_block,
           residual_ciphertext_block - AES_BLOCK_SIZE, residual_block_size);
    memcpy(residual_ciphertext_block - AES_BLOCK_SIZE,
           encrypted_residual_block.data(), AES_BLOCK_SIZE);
  }
  return true;
}

void AesCbcEncryptor::SetIvInternal() {
  internal_iv_ = iv();
  internal_iv_.resize(AES_BLOCK_SIZE, 0);
}

size_t AesCbcEncryptor::NumPaddingBytes(size_t size) const {
  return (padding_scheme_ == kPkcs5Padding)
             ? (AES_BLOCK_SIZE - (size % AES_BLOCK_SIZE))
             : 0;
}

void AesCbcEncryptor::CbcEncryptBlocks(const uint8_t* plaintext,
                                       size_t plaintext_size,
                                       uint8_t* ciphertext) {
  CHECK_EQ(plaintext_size % AES_BLOCK_SIZE, 0u);

  size_t output_size = 0;
  CHECK_EQ(
      mbedtls_cipher_crypt(&cipher_ctx_, internal_iv_.data(), AES_BLOCK_SIZE,
                           plaintext, plaintext_size, ciphertext, &output_size),
      0);

  CHECK_EQ(output_size % AES_BLOCK_SIZE, 0u);
  CHECK_GT(output_size, 0u);

  uint8_t* last_block = ciphertext + output_size - AES_BLOCK_SIZE;
  internal_iv_.assign(last_block, last_block + AES_BLOCK_SIZE);
}

}  // namespace media
}  // namespace shaka
