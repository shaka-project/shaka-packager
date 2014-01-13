// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryptor_source.h"

#include "media/base/aes_encryptor.h"

namespace {
// Generate 64bit IV by default.
const size_t kDefaultIvSize = 8u;

const uint8 kWidevineSystemId[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6,
                                   0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc,
                                   0xd5, 0x1d, 0x21, 0xed};
}  // namespace

namespace media {

EncryptorSource::EncryptorSource()
    : iv_size_(kDefaultIvSize),
      key_system_id_(kWidevineSystemId,
                     kWidevineSystemId + arraysize(kWidevineSystemId)) {}

EncryptorSource::~EncryptorSource() {}

scoped_ptr<AesCtrEncryptor> EncryptorSource::CreateEncryptor() {
  scoped_ptr<AesCtrEncryptor> encryptor(new AesCtrEncryptor());
  const bool initialized =
      iv_.empty() ? encryptor->InitializeWithRandomIv(key_, iv_size_)
                  : encryptor->InitializeWithIv(key_, iv_);
  if (!initialized) {
    LOG(ERROR) << "Failed to the initialize encryptor.";
    return scoped_ptr<AesCtrEncryptor>();
  }
  return encryptor.Pass();
}

}  // namespace media
