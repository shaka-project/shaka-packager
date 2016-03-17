// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/aes_decryptor.h"

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "packager/base/logging.h"

namespace {

// AES defines three key sizes: 128, 192 and 256 bits.
bool IsKeySizeValidForAes(size_t key_size) {
  return key_size == 16 || key_size == 24 || key_size == 32;
}

}  // namespace

namespace edash_packager {
namespace media {

AesDecryptor::AesDecryptor() {}
AesDecryptor::~AesDecryptor() {}

AesCtrDecryptor::AesCtrDecryptor() {}

AesCtrDecryptor::~AesCtrDecryptor() {}

bool AesCtrDecryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& iv) {
  encryptor_.reset(new AesCtrEncryptor);
  return encryptor_->InitializeWithIv(key, iv);
}

// For AES CTR, encryption and decryption are identical.
bool AesCtrDecryptor::Decrypt(const uint8_t* ciphertext,
                              size_t ciphertext_size,
                              uint8_t* plaintext) {
  DCHECK(encryptor_);
  return encryptor_->EncryptData(ciphertext, ciphertext_size, plaintext);
}

bool AesCtrDecryptor::Decrypt(const std::vector<uint8_t>& ciphertext,
                              std::vector<uint8_t>* plaintext) {
  DCHECK(encryptor_);
  return encryptor_->Encrypt(ciphertext, plaintext);
}

bool AesCtrDecryptor::Decrypt(const std::string& ciphertext,
                              std::string* plaintext) {
  DCHECK(encryptor_);
  return encryptor_->Encrypt(ciphertext, plaintext);
}

bool AesCtrDecryptor::SetIv(const std::vector<uint8_t>& iv) {
  DCHECK(encryptor_);
  return encryptor_->SetIv(iv);
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

  aes_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_decrypt_key(&key[0], key.size() * 8, aes_key_.get()), 0);

  iv_ = iv;
  return true;
}

bool AesCbcPkcs5Decryptor::Decrypt(const uint8_t* ciphertext,
                                   size_t ciphertext_size,
                                   uint8_t* plaintext) {
  NOTIMPLEMENTED();
  return false;
}

bool AesCbcPkcs5Decryptor::Decrypt(const std::vector<uint8_t>& ciphertext,
                                   std::vector<uint8_t>* plaintext) {
  NOTIMPLEMENTED();
  return false;
}

bool AesCbcPkcs5Decryptor::Decrypt(const std::string& ciphertext,
                                   std::string* plaintext) {
  if ((ciphertext.size() % AES_BLOCK_SIZE) != 0) {
    LOG(ERROR) << "Expecting cipher text size to be multiple of "
               << AES_BLOCK_SIZE << ", got " << ciphertext.size();
    return false;
  }

  DCHECK(plaintext);
  DCHECK(aes_key_);

  plaintext->resize(ciphertext.size());
  AES_cbc_encrypt(reinterpret_cast<const uint8_t*>(ciphertext.data()),
                  reinterpret_cast<uint8_t*>(string_as_array(plaintext)),
                  ciphertext.size(),
                  aes_key_.get(),
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

  aes_key_.reset(new AES_KEY());
  CHECK_EQ(AES_set_decrypt_key(&key[0], key.size() * 8, aes_key_.get()), 0);

  iv_ = iv;
  return true;
}

bool AesCbcCtsDecryptor::Decrypt(const uint8_t* ciphertext,
                                 size_t ciphertext_size,
                                 uint8_t* plaintext) {
  DCHECK(ciphertext);
  DCHECK(plaintext);

  if (ciphertext_size < AES_BLOCK_SIZE) {
    // Don't have a full block, leave unencrypted.
    memcpy(plaintext, ciphertext, ciphertext_size);
    return true;
  }

  std::vector<uint8_t> iv(iv_);
  size_t residual_block_size = ciphertext_size % AES_BLOCK_SIZE;

  if (residual_block_size == 0) {
    // No residual block. No need to do ciphertext stealing.
    AES_cbc_encrypt(ciphertext,
                    plaintext,
                    ciphertext_size,
                    aes_key_.get(),
                    &iv[0],
                    AES_DECRYPT);
    return true;
  }

  // AES-CBC decrypt everything up to the next-to-last full block.
  size_t cbc_size = ciphertext_size - residual_block_size;
  if (cbc_size > AES_BLOCK_SIZE) {
    AES_cbc_encrypt(ciphertext,
                    plaintext,
                    cbc_size - AES_BLOCK_SIZE,
                    aes_key_.get(),
                    &iv[0],
                    AES_DECRYPT);
  }

  // Determine what the last IV should be so that we can "skip ahead" in the
  // CBC decryption.
  std::vector<uint8_t> last_iv(
      ciphertext + ciphertext_size - residual_block_size,
      ciphertext + ciphertext_size);
  last_iv.resize(AES_BLOCK_SIZE, 0);

  // Decrypt the next-to-last block using the IV determined above. This decrypts
  // the residual block bits.
  AES_cbc_encrypt(
      ciphertext + ciphertext_size - residual_block_size - AES_BLOCK_SIZE,
      plaintext + ciphertext_size - residual_block_size - AES_BLOCK_SIZE,
      AES_BLOCK_SIZE, aes_key_.get(), &last_iv[0], AES_DECRYPT);

  // Swap back the residual block bits and the next-to-last full block.
  if (plaintext == ciphertext) {
    uint8_t* ptr1 = plaintext + ciphertext_size - residual_block_size;
    uint8_t* ptr2 = plaintext + ciphertext_size - residual_block_size - AES_BLOCK_SIZE;
    for (size_t i = 0; i < residual_block_size; ++i) {
      uint8_t temp = *ptr1;
      *ptr1 = *ptr2;
      *ptr2 = temp;
      ++ptr1;
      ++ptr2;
    }
  } else {
    uint8_t* residual_plaintext_block =
        plaintext + ciphertext_size - residual_block_size;
    memcpy(residual_plaintext_block, residual_plaintext_block - AES_BLOCK_SIZE,
           residual_block_size);
    memcpy(residual_plaintext_block - AES_BLOCK_SIZE,
           ciphertext + ciphertext_size - residual_block_size,
           residual_block_size);
  }

  // Decrypt the last full block.
  AES_cbc_encrypt(
      plaintext + ciphertext_size - residual_block_size - AES_BLOCK_SIZE,
      plaintext + ciphertext_size - residual_block_size - AES_BLOCK_SIZE,
      AES_BLOCK_SIZE, aes_key_.get(), &iv[0], AES_DECRYPT);
  return true;
}

bool AesCbcCtsDecryptor::Decrypt(const std::vector<uint8_t>& ciphertext,
                                 std::vector<uint8_t>* plaintext) {
  DCHECK(plaintext);

  plaintext->resize(ciphertext.size(), 0);
  if (ciphertext.empty())
    return true;

  return Decrypt(ciphertext.data(), ciphertext.size(), &(*plaintext)[0]);
}

bool AesCbcCtsDecryptor::Decrypt(const std::string& ciphertext,
                                 std::string* plaintext) {
  NOTIMPLEMENTED();
  return false;
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
