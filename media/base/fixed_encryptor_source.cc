// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fixed_encryptor_source.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/aes_encryptor.h"

namespace {
// The size of generated IV for this encryptor source.
const uint8 kIvSize = 8;
}  // namespace

namespace media {

FixedEncryptorSource::FixedEncryptorSource(const std::string& key_id_hex,
                                           const std::string& key_hex,
                                           const std::string& pssh_hex,
                                           uint32 clear_milliseconds)
    : key_id_hex_(key_id_hex),
      key_hex_(key_hex),
      pssh_hex_(pssh_hex) {
  set_clear_milliseconds(clear_milliseconds);
}

FixedEncryptorSource::~FixedEncryptorSource() {}

Status FixedEncryptorSource::Initialize() {
  std::vector<uint8> key_id;
  if (!base::HexStringToBytes(key_id_hex_, &key_id)) {
    LOG(ERROR) << "Cannot parse key_id_hex " << key_id_hex_;
    return Status(error::INVALID_ARGUMENT, "Cannot parse input key_id_hex.");
  }

  std::vector<uint8> key;
  if (!base::HexStringToBytes(key_hex_, &key)) {
    LOG(ERROR) << "Cannot parse key_hex " << key_hex_;
    return Status(error::INVALID_ARGUMENT, "Cannot parse input key_hex.");
  }

  std::vector<uint8> pssh;
  if (!base::HexStringToBytes(pssh_hex_, &pssh)) {
    LOG(ERROR) << "Cannot parse pssh_hex " << pssh_hex_;
    return Status(error::INVALID_ARGUMENT, "Cannot parse input pssh_hex.");
  }

  scoped_ptr<AesCtrEncryptor> encryptor(new AesCtrEncryptor());
  if (!encryptor->InitializeWithRandomIv(key, kIvSize))
    return Status(error::UNKNOWN, "Failed to initialize the encryptor.");

  set_encryptor(encryptor.Pass());
  set_key_id(key_id);
  set_key(key);
  set_pssh(pssh);
  return Status::OK;
}

}  // namespace media
