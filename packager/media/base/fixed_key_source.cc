// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/fixed_key_source.h"

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"

namespace shaka {
namespace media {

FixedKeySource::~FixedKeySource() {}

Status FixedKeySource::FetchKeys(const std::vector<uint8_t>& pssh_box) {
  // Do nothing for fixed key encryption/decryption.
  return Status::OK;
}

Status FixedKeySource::FetchKeys(
    const std::vector<std::vector<uint8_t>>& key_ids) {
  // Do nothing for fixed key encryption/decryption.
  return Status::OK;
}

Status FixedKeySource::FetchKeys(uint32_t asset_id) {
  // Do nothing for fixed key encryption/decryption.
  return Status::OK;
}

Status FixedKeySource::GetKey(TrackType track_type, EncryptionKey* key) {
  DCHECK(key);
  DCHECK(encryption_key_);
  *key = *encryption_key_;
  return Status::OK;
}

Status FixedKeySource::GetKey(const std::vector<uint8_t>& key_id,
                              EncryptionKey* key) {
  DCHECK(key);
  DCHECK(encryption_key_);
  if (key_id != encryption_key_->key_id) {
    return Status(error::NOT_FOUND,
                  std::string("Key for key ID ") +
                      base::HexEncode(&key_id[0], key_id.size()) +
                      " was not found.");
  }
  *key = *encryption_key_;
  return Status::OK;
}

Status FixedKeySource::GetCryptoPeriodKey(uint32_t crypto_period_index,
                                          TrackType track_type,
                                          EncryptionKey* key) {
  // Create a copy of the key.
  *key = *encryption_key_;

  // A naive key rotation algorithm is implemented here by left rotating the
  // key, key_id and pssh. Note that this implementation is only intended for
  // testing purpose. The actual key rotation algorithm can be much more
  // complicated.
  LOG(WARNING)
      << "This naive key rotation algorithm should not be used in production.";
  std::rotate(key->key_id.begin(),
              key->key_id.begin() + (crypto_period_index % key->key_id.size()),
              key->key_id.end());
  std::rotate(key->key.begin(),
              key->key.begin() + (crypto_period_index % key->key.size()),
              key->key.end());

  for (size_t i = 0; i < key->key_system_info.size(); i++) {
    std::vector<uint8_t> pssh_data = key->key_system_info[i].pssh_data();
    std::rotate(pssh_data.begin(),
                pssh_data.begin() + (crypto_period_index % pssh_data.size()),
                pssh_data.end());
    key->key_system_info[i].set_pssh_data(pssh_data);
  }

  return Status::OK;
}

scoped_ptr<FixedKeySource> FixedKeySource::CreateFromHexStrings(
    const std::string& key_id_hex,
    const std::string& key_hex,
    const std::string& pssh_boxes_hex,
    const std::string& iv_hex) {
  scoped_ptr<EncryptionKey> encryption_key(new EncryptionKey());

  if (!base::HexStringToBytes(key_id_hex, &encryption_key->key_id)) {
    LOG(ERROR) << "Cannot parse key_id_hex " << key_id_hex;
    return scoped_ptr<FixedKeySource>();
  } else if (encryption_key->key_id.size() != 16) {
    LOG(ERROR) << "Invalid key ID size '" << encryption_key->key_id.size()
               << "', must be 16 bytes.";
    return scoped_ptr<FixedKeySource>();
  }

  if (!base::HexStringToBytes(key_hex, &encryption_key->key)) {
    LOG(ERROR) << "Cannot parse key_hex " << key_hex;
    return scoped_ptr<FixedKeySource>();
  }

  std::vector<uint8_t> pssh_boxes;
  if (!pssh_boxes_hex.empty() &&
      !base::HexStringToBytes(pssh_boxes_hex, &pssh_boxes)) {
    LOG(ERROR) << "Cannot parse pssh_hex " << pssh_boxes_hex;
    return scoped_ptr<FixedKeySource>();
  }

  if (!iv_hex.empty()) {
    if (!base::HexStringToBytes(iv_hex, &encryption_key->iv)) {
      LOG(ERROR) << "Cannot parse iv_hex " << iv_hex;
      return scoped_ptr<FixedKeySource>();
    }
  }

  if (!ProtectionSystemSpecificInfo::ParseBoxes(
          pssh_boxes.data(), pssh_boxes.size(),
          &encryption_key->key_system_info)) {
    LOG(ERROR) << "--pssh argument should be full PSSH boxes.";
    return scoped_ptr<FixedKeySource>();
  }

  // If there aren't any PSSH boxes given, create one with the common system ID.
  if (encryption_key->key_system_info.size() == 0) {
    ProtectionSystemSpecificInfo info;
    info.add_key_id(encryption_key->key_id);
    info.set_system_id(kCommonSystemId, arraysize(kCommonSystemId));
    info.set_pssh_box_version(1);

    encryption_key->key_system_info.push_back(info);
  }

  return scoped_ptr<FixedKeySource>(new FixedKeySource(encryption_key.Pass()));
}

FixedKeySource::FixedKeySource() {}
FixedKeySource::FixedKeySource(scoped_ptr<EncryptionKey> key)
    : encryption_key_(key.Pass()) {}

}  // namespace media
}  // namespace shaka
