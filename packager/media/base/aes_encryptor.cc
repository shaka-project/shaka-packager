// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/aes_encryptor.h"

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "packager/base/logging.h"

namespace {

// Increment an 8-byte counter by 1. Return true if overflowed.
bool Increment64(uint8_t* counter) {
  DCHECK(counter);
  for (int i = 7; i >= 0; --i)
    if (++counter[i] != 0)
      return false;
  return true;
}

// According to ISO/IEC FDIS 23001-7: CENC spec, IV should be either
// 64-bit (8-byte) or 128-bit (16-byte).
bool IsIvSizeValid(size_t iv_size) { return iv_size == 8 || iv_size == 16; }

// AES defines three key sizes: 128, 192 and 256 bits.
bool IsKeySizeValidForAes(size_t key_size) {
  return key_size == 16 || key_size == 24 || key_size == 32;
}

// CENC protection scheme uses 128-bit keys in counter mode.
const uint32_t kCencKeySize = 16;

}  // namespace

namespace edash_packager {
namespace media {

AesEncryptor::AesEncryptor() {}
AesEncryptor::~AesEncryptor() {}

bool AesEncryptor::InitializeWithRandomIv(
    const std::vector<uint8_t>& key,
    uint8_t iv_size) {
  std::vector<uint8_t> iv(iv_size, 0);
  if (RAND_bytes(&iv[0], iv_size) != 1) {
    LOG(ERROR) << "RAND_bytes failed with error: "
               << ERR_error_string(ERR_get_error(), NULL);
    return false;
  }
  return InitializeWithIv(key, iv);
}

bool AesEncryptor::Encrypt(const std::vector<uint8_t>& plaintext,
                           std::vector<uint8_t>* ciphertext) {
  if (plaintext.empty())
    return true;
  ciphertext->resize(plaintext.size() + NumPaddingBytes(plaintext.size()));
  return EncryptData(plaintext.data(), plaintext.size(), ciphertext->data());
}

bool AesEncryptor::Encrypt(const std::string& plaintext,
                           std::string* ciphertext) {
  ciphertext->resize(plaintext.size() + NumPaddingBytes(plaintext.size()));
  return EncryptData(reinterpret_cast<const uint8_t*>(plaintext.data()),
                     plaintext.size(),
                     reinterpret_cast<uint8_t*>(string_as_array(ciphertext)));
}

AesCtrEncryptor::AesCtrEncryptor()
    : block_offset_(0),
      encrypted_counter_(AES_BLOCK_SIZE, 0),
      counter_overflow_(false) {
  COMPILE_ASSERT(AES_BLOCK_SIZE == kCencKeySize,
                 cenc_key_size_should_be_the_same_as_aes_block_size);
}

AesCtrEncryptor::~AesCtrEncryptor() {}

bool AesCtrEncryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& iv) {
  if (key.size() != kCencKeySize) {
    LOG(ERROR) << "Invalid key size of " << key.size() << " for CENC.";
    return false;
  }
  if (!IsIvSizeValid(iv.size())) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  aes_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_encrypt_key(&key[0], AES_BLOCK_SIZE * 8, aes_key_.get()), 0);
  return SetIv(iv);
}

size_t AesCtrEncryptor::NumPaddingBytes(size_t size) {
  return 0;
}

bool AesCtrEncryptor::EncryptData(const uint8_t* plaintext,
                                  size_t plaintext_size,
                                  uint8_t* ciphertext) {
  DCHECK(plaintext);
  DCHECK(ciphertext);
  DCHECK(aes_key_);

  for (size_t i = 0; i < plaintext_size; ++i) {
    if (block_offset_ == 0) {
      AES_encrypt(&counter_[0], &encrypted_counter_[0], aes_key_.get());
      // As mentioned in ISO/IEC FDIS 23001-7: CENC spec, of the 16 byte counter
      // block, bytes 8 to 15 (i.e. the least significant bytes) are used as a
      // simple 64 bit unsigned integer that is incremented by one for each
      // subsequent block of sample data processed and is kept in network byte
      // order.
      if (Increment64(&counter_[8]))
        counter_overflow_ = true;
    }
    ciphertext[i] = plaintext[i] ^ encrypted_counter_[block_offset_];
    block_offset_ = (block_offset_ + 1) % AES_BLOCK_SIZE;
  }
  return true;
}

void AesCtrEncryptor::UpdateIv() {
  block_offset_ = 0;

  // As recommended in ISO/IEC FDIS 23001-7: CENC spec, for 64-bit (8-byte)
  // IV_Sizes, initialization vectors for subsequent samples can be created by
  // incrementing the initialization vector of the previous sample.
  // For 128-bit (16-byte) IV_Sizes, initialization vectors for subsequent
  // samples should be created by adding the block count of the previous sample
  // to the initialization vector of the previous sample.
  if (iv_.size() == 8) {
    Increment64(&iv_[0]);
    counter_ = iv_;
    counter_.resize(AES_BLOCK_SIZE, 0);
  } else {
    DCHECK_EQ(16u, iv_.size());
    // Even though the block counter portion of the counter (bytes 8 to 15) is
    // treated as a 64-bit number, it is recommended that the initialization
    // vector is treated as a 128-bit number when calculating the next
    // initialization vector from the previous one. The block counter portion
    // is already incremented by number of blocks, the other 64 bits of the
    // counter (bytes 0 to 7) is incremented here if the block counter portion
    // has overflowed.
    if (counter_overflow_)
      Increment64(&counter_[0]);
    iv_ = counter_;
  }
  counter_overflow_ = false;
}

bool AesCtrEncryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (!IsIvSizeValid(iv.size())) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  block_offset_ = 0;
  counter_ = iv_ = iv;
  counter_.resize(AES_BLOCK_SIZE, 0);
  return true;
}

AesCbcPkcs5Encryptor::AesCbcPkcs5Encryptor() {}
AesCbcPkcs5Encryptor::~AesCbcPkcs5Encryptor() {}

bool AesCbcPkcs5Encryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                            const std::vector<uint8_t>& iv) {
  if (!IsKeySizeValidForAes(key.size())) {
    LOG(ERROR) << "Invalid AES key size: " << key.size();
    return false;
  }
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  aes_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_encrypt_key(&key[0], key.size() * 8, aes_key_.get()), 0);

  iv_ = iv;
  return true;
}

size_t AesCbcPkcs5Encryptor::NumPaddingBytes(size_t size) {
  return AES_BLOCK_SIZE - (size % AES_BLOCK_SIZE);
}

bool AesCbcPkcs5Encryptor::EncryptData(const uint8_t* plaintext,
                                       size_t plaintext_size,
                                       uint8_t* ciphertext) {
  DCHECK(ciphertext);
  DCHECK(aes_key_);

  // Pad the input with PKCS5 padding.
  // TODO(kqyang): Consider more efficient implementation.
  memcpy(ciphertext, plaintext, plaintext_size);
  for (size_t i = plaintext_size;
       i < plaintext_size + NumPaddingBytes(plaintext_size); ++i) {
    ciphertext[i] = NumPaddingBytes(plaintext_size);
  }

  std::vector<uint8_t> iv(iv_);
  AES_cbc_encrypt(ciphertext, ciphertext,
                  plaintext_size + NumPaddingBytes(plaintext_size),
                  aes_key_.get(), &iv[0], AES_ENCRYPT);
  return true;
}

void AesCbcPkcs5Encryptor::UpdateIv() {}

bool AesCbcPkcs5Encryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  iv_ = iv;
  return true;
}

AesCbcCtsEncryptor::AesCbcCtsEncryptor() {}
AesCbcCtsEncryptor::~AesCbcCtsEncryptor() {}

bool AesCbcCtsEncryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& iv) {
  if (!IsKeySizeValidForAes(key.size())) {
    LOG(ERROR) << "Invalid AES key size: " << key.size();
    return false;
  }
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  aes_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_encrypt_key(&key[0], key.size() * 8, aes_key_.get()), 0);

  iv_ = iv;
  return true;
}

size_t AesCbcCtsEncryptor::NumPaddingBytes(size_t size) {
  return 0;
}

bool AesCbcCtsEncryptor::EncryptData(const uint8_t* plaintext,
                                     size_t size,
                                     uint8_t* ciphertext) {
  DCHECK(plaintext);
  DCHECK(ciphertext);

  if (size < AES_BLOCK_SIZE) {
    // Don't have a full block, leave unencrypted.
    memcpy(ciphertext, plaintext, size);
    return true;
  }

  std::vector<uint8_t> iv(iv_);
  size_t residual_block_size = size % AES_BLOCK_SIZE;
  size_t cbc_size = size - residual_block_size;

  // Encrypt everything but the residual block using CBC.
  AES_cbc_encrypt(plaintext,
                  ciphertext,
                  cbc_size,
                  aes_key_.get(),
                  &iv[0],
                  AES_ENCRYPT);
  if (residual_block_size == 0) {
    // No residual block. No need to do ciphertext stealing.
    return true;
  }

  // Zero-pad the residual block and encrypt using CBC.
  std::vector<uint8_t> residual_block(plaintext + size - residual_block_size,
                                      plaintext + size);
  residual_block.resize(AES_BLOCK_SIZE, 0);
  AES_cbc_encrypt(&residual_block[0],
                  &residual_block[0],
                  AES_BLOCK_SIZE,
                  aes_key_.get(),
                  &iv[0],
                  AES_ENCRYPT);

  // Replace the last full block with the zero-padded, encrypted residual block,
  // and replace the residual block with the equivalent portion of the last full
  // encrypted block. It may appear that some encrypted bits of the last full
  // block are lost, but they are not, as they were used as the IV when
  // encrypting the zero-padded residual block.
  uint8_t* residual_ciphertext_block = ciphertext + size - residual_block_size;
  memcpy(residual_ciphertext_block,
         residual_ciphertext_block - AES_BLOCK_SIZE,
         residual_block_size);
  memcpy(residual_ciphertext_block - AES_BLOCK_SIZE,
         residual_block.data(),
         AES_BLOCK_SIZE);
  return true;
}

void AesCbcCtsEncryptor::UpdateIv() {}

bool AesCbcCtsEncryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  iv_ = iv;
  return true;
}

}  // namespace media
}  // namespace edash_packager
