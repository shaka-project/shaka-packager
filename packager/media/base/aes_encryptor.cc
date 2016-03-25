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

}  // namespace

namespace edash_packager {
namespace media {

AesEncryptor::AesEncryptor() {}
AesEncryptor::~AesEncryptor() {}

bool AesEncryptor::InitializeWithRandomIv(
    const std::vector<uint8_t>& key,
    uint8_t iv_size) {
  std::vector<uint8_t> iv(iv_size, 0);
  if (RAND_bytes(iv.data(), iv_size) != 1) {
    LOG(ERROR) << "RAND_bytes failed with error: "
               << ERR_error_string(ERR_get_error(), NULL);
    return false;
  }
  return InitializeWithIv(key, iv);
}

bool AesEncryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                    const std::vector<uint8_t>& iv) {
  if (!IsKeySizeValidForAes(key.size())) {
    LOG(ERROR) << "Invalid AES key size: " << key.size();
    return false;
  }

  aes_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_encrypt_key(key.data(), key.size() * 8, aes_key_.get()), 0);
  return SetIv(iv);
}

bool AesEncryptor::Encrypt(const std::vector<uint8_t>& plaintext,
                           std::vector<uint8_t>* ciphertext) {
  // Save plaintext size to make it work for in-place conversion, since the
  // next statement will update the plaintext size.
  const size_t plaintext_size = plaintext.size();
  ciphertext->resize(plaintext_size + NumPaddingBytes(plaintext.size()));
  return EncryptInternal(plaintext.data(), plaintext_size, ciphertext->data());
}

bool AesEncryptor::Encrypt(const std::string& plaintext,
                           std::string* ciphertext) {
  // Save plaintext size to make it work for in-place conversion, since the
  // next statement will update the plaintext size.
  const size_t plaintext_size = plaintext.size();
  ciphertext->resize(plaintext_size + NumPaddingBytes(plaintext.size()));
  return EncryptInternal(
      reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext_size,
      reinterpret_cast<uint8_t*>(string_as_array(ciphertext)));
}

AesCtrEncryptor::AesCtrEncryptor()
    : block_offset_(0),
      encrypted_counter_(AES_BLOCK_SIZE, 0),
      counter_overflow_(false) {}

AesCtrEncryptor::~AesCtrEncryptor() {}

void AesCtrEncryptor::UpdateIv() {
  block_offset_ = 0;

  // As recommended in ISO/IEC FDIS 23001-7: CENC spec, for 64-bit (8-byte)
  // IV_Sizes, initialization vectors for subsequent samples can be created by
  // incrementing the initialization vector of the previous sample.
  // For 128-bit (16-byte) IV_Sizes, initialization vectors for subsequent
  // samples should be created by adding the block count of the previous sample
  // to the initialization vector of the previous sample.
  if (iv().size() == 8) {
    counter_ = iv();
    Increment64(&counter_[0]);
    set_iv(counter_);
    counter_.resize(AES_BLOCK_SIZE, 0);
  } else {
    DCHECK_EQ(16u, iv().size());
    // Even though the block counter portion of the counter (bytes 8 to 15) is
    // treated as a 64-bit number, it is recommended that the initialization
    // vector is treated as a 128-bit number when calculating the next
    // initialization vector from the previous one. The block counter portion
    // is already incremented by number of blocks, the other 64 bits of the
    // counter (bytes 0 to 7) is incremented here if the block counter portion
    // has overflowed.
    if (counter_overflow_)
      Increment64(&counter_[0]);
    set_iv(counter_);
  }
  counter_overflow_ = false;
}

bool AesCtrEncryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (!IsIvSizeValid(iv.size())) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  block_offset_ = 0;
  set_iv(iv);
  counter_ = iv;
  counter_.resize(AES_BLOCK_SIZE, 0);
  return true;
}

bool AesCtrEncryptor::EncryptInternal(const uint8_t* plaintext,
                                      size_t plaintext_size,
                                      uint8_t* ciphertext) {
  DCHECK(plaintext);
  DCHECK(ciphertext);
  DCHECK(aes_key());

  for (size_t i = 0; i < plaintext_size; ++i) {
    if (block_offset_ == 0) {
      AES_encrypt(&counter_[0], &encrypted_counter_[0], aes_key());
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

size_t AesCtrEncryptor::NumPaddingBytes(size_t size) const {
  // No padding needed for CTR.
  return 0;
}

AesCbcEncryptor::AesCbcEncryptor(CbcPaddingScheme padding_scheme,
                                 bool chain_across_calls)
    : padding_scheme_(padding_scheme),
      chain_across_calls_(chain_across_calls) {
  if (padding_scheme_ != kNoPadding) {
    CHECK(!chain_across_calls) << "cipher block chain across calls only makes "
                                  "sense if the padding_scheme is kNoPadding.";
  }
}
AesCbcEncryptor::~AesCbcEncryptor() {}

void AesCbcEncryptor::UpdateIv() {
  // From CENC spec: CBC mode Initialization Vectors need not be unique per
  // sample or Subsample and may be generated randomly or sequentially, e.g.
  // a per sample IV may be (1) equal to the cipher text of the last encrypted
  // cipher block (a continous cipher block chain across samples), or (2)
  // generated by incrementing the previuos IV by the number of cipher blocks in the last
  // sample or (3) by a fixed amount. We use method (1) here. No separate IV
  // update is needed.
}

bool AesCbcEncryptor::SetIv(const std::vector<uint8_t>& iv) {
  if (iv.size() != AES_BLOCK_SIZE) {
    LOG(ERROR) << "Invalid IV size: " << iv.size();
    return false;
  }

  set_iv(iv);
  return true;
}

bool AesCbcEncryptor::EncryptInternal(const uint8_t* plaintext,
                                      size_t plaintext_size,
                                      uint8_t* ciphertext) {
  DCHECK(aes_key());

  const size_t residual_block_size = plaintext_size % AES_BLOCK_SIZE;
  if (padding_scheme_ == kNoPadding && residual_block_size != 0) {
    LOG(ERROR) << "Expecting input size to be multiple of " << AES_BLOCK_SIZE
               << ", got " << plaintext_size;
    return false;
  }

  // Encrypt everything but the residual block using CBC.
  const size_t cbc_size = plaintext_size - residual_block_size;
  std::vector<uint8_t> local_iv(iv());
  if (cbc_size != 0) {
    AES_cbc_encrypt(plaintext, ciphertext, cbc_size, aes_key(), local_iv.data(),
                    AES_ENCRYPT);
  } else if (padding_scheme_ == kCtsPadding) {
    // Don't have a full block, leave unencrypted.
    memcpy(ciphertext, plaintext, plaintext_size);
    return true;
  }
  if (residual_block_size == 0 && padding_scheme_ != kPkcs5Padding) {
    if (chain_across_calls_)
      set_iv(local_iv);
    // No residual block. No need to do padding.
    return true;
  }
  DCHECK(!chain_across_calls_);

  std::vector<uint8_t> residual_block(plaintext + cbc_size,
                                      plaintext + plaintext_size);
  DCHECK_EQ(residual_block.size(), residual_block_size);
  uint8_t* residual_ciphertext_block = ciphertext + cbc_size;

  if (padding_scheme_ == kPkcs5Padding) {
    const size_t num_padding_bytes = AES_BLOCK_SIZE - residual_block_size;
    DCHECK_EQ(num_padding_bytes, NumPaddingBytes(plaintext_size));
    // Pad residue block with PKCS5 padding.
    residual_block.resize(AES_BLOCK_SIZE, static_cast<char>(num_padding_bytes));
    AES_cbc_encrypt(residual_block.data(), residual_ciphertext_block,
                    AES_BLOCK_SIZE, aes_key(), local_iv.data(), AES_ENCRYPT);
  } else {
    DCHECK_EQ(padding_scheme_, kCtsPadding);

    // Zero-pad the residual block and encrypt using CBC.
    residual_block.resize(AES_BLOCK_SIZE, 0);
    AES_cbc_encrypt(residual_block.data(), residual_block.data(),
                    AES_BLOCK_SIZE, aes_key(), local_iv.data(), AES_ENCRYPT);

    // Replace the last full block with the zero-padded, encrypted residual
    // block, and replace the residual block with the equivalent portion of the
    // last full encrypted block. It may appear that some encrypted bits of the
    // last full block are lost, but they are not, as they were used as the IV
    // when encrypting the zero-padded residual block.
    memcpy(residual_ciphertext_block,
           residual_ciphertext_block - AES_BLOCK_SIZE, residual_block_size);
    memcpy(residual_ciphertext_block - AES_BLOCK_SIZE, residual_block.data(),
           AES_BLOCK_SIZE);
  }
  return true;
}

size_t AesCbcEncryptor::NumPaddingBytes(size_t size) const {
  return (padding_scheme_ == kPkcs5Padding)
             ? (AES_BLOCK_SIZE - (size % AES_BLOCK_SIZE))
             : 0;
}

}  // namespace media
}  // namespace edash_packager
