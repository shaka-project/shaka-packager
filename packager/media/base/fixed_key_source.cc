// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/fixed_key_source.h"

#include <algorithm>
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"

namespace shaka {
namespace media {

FixedKeySource::~FixedKeySource() {}

Status FixedKeySource::FetchKeys(EmeInitDataType init_data_type,
                                 const std::vector<uint8_t>& init_data) {
  // Do nothing for fixed key encryption/decryption.
  return Status::OK;
}

Status FixedKeySource::GetKey(const std::string& stream_label,
                              EncryptionKey* key) {
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
                                          const std::string& stream_label,
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
    if (!pssh_data.empty()) {
      std::rotate(pssh_data.begin(),
                  pssh_data.begin() + (crypto_period_index % pssh_data.size()),
                  pssh_data.end());
      key->key_system_info[i].set_pssh_data(pssh_data);
    }
  }

  return Status::OK;
}

std::unique_ptr<FixedKeySource> FixedKeySource::Create(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& pssh_boxes,
    const std::vector<uint8_t>& iv) {
  std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey());

  if (key_id.size() != 16) {
    LOG(ERROR) << "Invalid key ID size '" << key_id.size()
               << "', must be 16 bytes.";
    return std::unique_ptr<FixedKeySource>();
  }
  if (key.size() != 16) {
    // CENC only supports AES-128, i.e. 16 bytes.
    LOG(ERROR) << "Invalid key size '" << key.size() << "', must be 16 bytes.";
    return std::unique_ptr<FixedKeySource>();
  }

  encryption_key->key_id = key_id;
  encryption_key->key = key;
  encryption_key->iv = iv;

  if (!ProtectionSystemSpecificInfo::ParseBoxes(
          pssh_boxes.data(), pssh_boxes.size(),
          &encryption_key->key_system_info)) {
    LOG(ERROR) << "--pssh argument should be full PSSH boxes.";
    return std::unique_ptr<FixedKeySource>();
  }

  // If there aren't any PSSH boxes given, create one with the common system ID.
  if (encryption_key->key_system_info.size() == 0) {
    ProtectionSystemSpecificInfo info;
    info.add_key_id(encryption_key->key_id);
    info.set_system_id(kCommonSystemId, arraysize(kCommonSystemId));
    info.set_pssh_box_version(1);

    encryption_key->key_system_info.push_back(info);
  }

  return std::unique_ptr<FixedKeySource>(
      new FixedKeySource(std::move(encryption_key)));
}

FixedKeySource::FixedKeySource() {}
FixedKeySource::FixedKeySource(std::unique_ptr<EncryptionKey> key)
    : encryption_key_(std::move(key)) {}

}  // namespace media
}  // namespace shaka
