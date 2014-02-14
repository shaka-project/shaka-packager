// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/fixed_encryptor_source.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace media {

FixedEncryptorSource::FixedEncryptorSource(const std::string& key_id_hex,
                                           const std::string& key_hex,
                                           const std::string& pssh_hex)
    : key_id_hex_(key_id_hex), key_hex_(key_hex), pssh_hex_(pssh_hex) {}

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

  set_key_id(key_id);
  set_key(key);
  set_pssh(pssh);
  return Status::OK;
}

}  // namespace media
