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

AesCtrEncryptor::AesCtrEncryptor()
    : block_offset_(0),
      encrypted_counter_(AES_BLOCK_SIZE, 0),
      counter_overflow_(false) {
  COMPILE_ASSERT(AES_BLOCK_SIZE == kCencKeySize,
                 cenc_key_size_should_be_the_same_as_aes_block_size);
}

AesCtrEncryptor::~AesCtrEncryptor() {}

bool AesCtrEncryptor::InitializeWithRandomIv(const std::vector<uint8_t>& key,
                                             uint8_t iv_size) {
  std::vector<uint8_t> iv(iv_size, 0);
  if (RAND_bytes(&iv[0], iv_size) != 1) {
    LOG(ERROR) << "RAND_bytes failed with error: "
               << ERR_error_string(ERR_get_error(), NULL);
    return false;
  }
  return InitializeWithIv(key, iv);
}

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

bool AesCtrEncryptor::Encrypt(const uint8_t* plaintext,
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

  encrypt_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_encrypt_key(&key[0], key.size() * 8, encrypt_key_.get()), 0);

  iv_ = iv;
  return true;
}

void AesCbcPkcs5Encryptor::Encrypt(const std::string& plaintext,
                                   std::string* ciphertext) {
  DCHECK(ciphertext);
  DCHECK(encrypt_key_);

  // Pad the input with PKCS5 padding.
  const size_t num_padding_bytes =
      AES_BLOCK_SIZE - (plaintext.size() % AES_BLOCK_SIZE);
  std::string padded_text = plaintext;
  padded_text.append(num_padding_bytes, static_cast<char>(num_padding_bytes));

  ciphertext->resize(padded_text.size());
  std::vector<uint8_t> iv(iv_);
  AES_cbc_encrypt(reinterpret_cast<const uint8_t*>(padded_text.data()),
                  reinterpret_cast<uint8_t*>(string_as_array(ciphertext)),
                  padded_text.size(),
                  encrypt_key_.get(),
                  &iv[0],
                  AES_ENCRYPT);
}

bool AesCbcPkcs5Encryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  iv_ = iv;
  return true;
}

AesCbcPkcs5Decryptor::AesCbcPkcs5Decryptor() {}
AesCbcPkcs5Decryptor::~AesCbcPkcs5Decryptor() {}

bool AesCbcPkcs5Decryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                            const std::vector<uint8_t>& iv) {
  if (!IsKeySizeValidForAes(key.size())) {
    LOG(ERROR) << "Invalid AES key size: " << key.size();
    return false;
  }
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  decrypt_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_decrypt_key(&key[0], key.size() * 8, decrypt_key_.get()), 0);

  iv_ = iv;
  return true;
}

bool AesCbcPkcs5Decryptor::Decrypt(const std::string& ciphertext,
                                   std::string* plaintext) {
  if ((ciphertext.size() % AES_BLOCK_SIZE) != 0) {
    LOG(ERROR) << "Expecting cipher text size to be multiple of "
               << AES_BLOCK_SIZE << ", got " << ciphertext.size();
    return false;
  }

  DCHECK(plaintext);
  DCHECK(decrypt_key_);

  plaintext->resize(ciphertext.size());
  AES_cbc_encrypt(reinterpret_cast<const uint8_t*>(ciphertext.data()),
                  reinterpret_cast<uint8_t*>(string_as_array(plaintext)),
                  ciphertext.size(),
                  decrypt_key_.get(),
                  &iv_[0],
                  AES_DECRYPT);

  // Strip off PKCS5 padding bytes.
  const uint8_t num_padding_bytes = (*plaintext)[plaintext->size() - 1];
  if (num_padding_bytes > AES_BLOCK_SIZE) {
    LOG(ERROR) << "Padding length is too large : "
               << static_cast<int>(num_padding_bytes);
    return false;
  }
  plaintext->resize(plaintext->size() - num_padding_bytes);
  return true;
}

bool AesCbcPkcs5Decryptor::SetIv(const std::vector<uint8_t>& iv) {
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

  encrypt_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_encrypt_key(&key[0], key.size() * 8, encrypt_key_.get()), 0);

  iv_ = iv;
  return true;
}

void AesCbcCtsEncryptor::Encrypt(const uint8_t* plaintext,
                                 size_t size,
                                 uint8_t* ciphertext) {
  DCHECK(plaintext);
  DCHECK(ciphertext);

  if (size < AES_BLOCK_SIZE) {
    // Don't have a full block, leave unencrypted.
    memcpy(ciphertext, plaintext, size);
    return;
  }

  std::vector<uint8_t> iv(iv_);
  size_t residual_block_size = size % AES_BLOCK_SIZE;
  size_t cbc_size = size - residual_block_size;

  // Encrypt everything but the residual block using CBC.
  AES_cbc_encrypt(plaintext,
                  ciphertext,
                  cbc_size,
                  encrypt_key_.get(),
                  &iv[0],
                  AES_ENCRYPT);
  if (residual_block_size == 0) {
    // No residual block. No need to do ciphertext stealing.
    return;
  }

  // Zero-pad the residual block and encrypt using CBC.
  std::vector<uint8_t> residual_block(plaintext + size - residual_block_size,
                                      plaintext + size);
  residual_block.resize(AES_BLOCK_SIZE, 0);
  AES_cbc_encrypt(&residual_block[0],
                  &residual_block[0],
                  AES_BLOCK_SIZE,
                  encrypt_key_.get(),
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
}

void AesCbcCtsEncryptor::Encrypt(const std::vector<uint8_t>& plaintext,
                                 std::vector<uint8_t>* ciphertext) {
  DCHECK(ciphertext);

  ciphertext->resize(plaintext.size(), 0);
  if (plaintext.empty())
    return;

  Encrypt(plaintext.data(), plaintext.size(), &(*ciphertext)[0]);
}

bool AesCbcCtsEncryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  iv_ = iv;
  return true;
}

AesCbcCtsDecryptor::AesCbcCtsDecryptor() {}
AesCbcCtsDecryptor::~AesCbcCtsDecryptor() {}

bool AesCbcCtsDecryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& iv) {
  if (!IsKeySizeValidForAes(key.size())) {
    LOG(ERROR) << "Invalid AES key size: " << key.size();
    return false;
  }
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  decrypt_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_decrypt_key(&key[0], key.size() * 8, decrypt_key_.get()), 0);

  iv_ = iv;
  return true;
}

void AesCbcCtsDecryptor::Decrypt(const uint8_t* ciphertext,
                                 size_t size,
                                 uint8_t* plaintext) {
  DCHECK(ciphertext);
  DCHECK(plaintext);

  if (size < AES_BLOCK_SIZE) {
    // Don't have a full block, leave unencrypted.
    memcpy(plaintext, ciphertext, size);
    return;
  }

  std::vector<uint8_t> iv(iv_);
  size_t residual_block_size = size % AES_BLOCK_SIZE;

  if (residual_block_size == 0) {
    // No residual block. No need to do ciphertext stealing.
    AES_cbc_encrypt(ciphertext,
                    plaintext,
                    size,
                    decrypt_key_.get(),
                    &iv[0],
                    AES_DECRYPT);
    return;
  }

  // AES-CBC decrypt everything up to the next-to-last full block.
  size_t cbc_size = size - residual_block_size;
  if (cbc_size > AES_BLOCK_SIZE) {
    AES_cbc_encrypt(ciphertext,
                    plaintext,
                    cbc_size - AES_BLOCK_SIZE,
                    decrypt_key_.get(),
                    &iv[0],
                    AES_DECRYPT);
  }

  // Determine what the last IV should be so that we can "skip ahead" in the
  // CBC decryption.
  std::vector<uint8_t> last_iv(ciphertext + size - residual_block_size,
                               ciphertext + size);
  last_iv.resize(AES_BLOCK_SIZE, 0);

  // Decrypt the next-to-last block using the IV determined above. This decrypts
  // the residual block bits.
  AES_cbc_encrypt(ciphertext + size - residual_block_size - AES_BLOCK_SIZE,
                  plaintext + size - residual_block_size - AES_BLOCK_SIZE,
                  AES_BLOCK_SIZE,
                  decrypt_key_.get(),
                  &last_iv[0],
                  AES_DECRYPT);

  // Swap back the residual block bits and the next-to-last full block.
  if (plaintext == ciphertext) {
    uint8_t* ptr1 = plaintext + size - residual_block_size;
    uint8_t* ptr2 = plaintext + size - residual_block_size - AES_BLOCK_SIZE;
    for (size_t i = 0; i < residual_block_size; ++i) {
      uint8_t temp = *ptr1;
      *ptr1 = *ptr2;
      *ptr2 = temp;
      ++ptr1;
      ++ptr2;
    }
  } else {
    uint8_t* residual_plaintext_block = plaintext + size - residual_block_size;
    memcpy(residual_plaintext_block,
           residual_plaintext_block - AES_BLOCK_SIZE,
           residual_block_size);
    memcpy(residual_plaintext_block - AES_BLOCK_SIZE,
           ciphertext + size - residual_block_size,
           residual_block_size);
  }

  // Decrypt the last full block.
  AES_cbc_encrypt(plaintext + size - residual_block_size - AES_BLOCK_SIZE,
                  plaintext + size - residual_block_size - AES_BLOCK_SIZE,
                  AES_BLOCK_SIZE,
                  decrypt_key_.get(),
                  &iv[0],
                  AES_DECRYPT);
}

void AesCbcCtsDecryptor::Decrypt(const std::vector<uint8_t>& ciphertext,
                                 std::vector<uint8_t>* plaintext) {
  DCHECK(plaintext);

  plaintext->resize(ciphertext.size(), 0);
  if (ciphertext.empty())
    return;

  Decrypt(ciphertext.data(), ciphertext.size(), &(*plaintext)[0]);
}

bool AesCbcCtsDecryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  iv_ = iv;
  return true;
}

}  // namespace media
}  // namespace edash_packager
