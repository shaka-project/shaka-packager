// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/raw_key_source.h"

#include <algorithm>
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/key_source.h"
#include "packager/status_macros.h"

namespace {
const char kEmptyDrmLabel[] = "";
}  // namespace

namespace shaka {
namespace media {

RawKeySource::~RawKeySource() {}

Status RawKeySource::FetchKeys(EmeInitDataType init_data_type,
                               const std::vector<uint8_t>& init_data) {
  // Do nothing for raw key encryption/decryption.
  return Status::OK;
}

Status RawKeySource::GetKey(const std::string& stream_label,
                            EncryptionKey* key) {
  DCHECK(key);
  // Try to find the key with label |stream_label|. If it is not available,
  // fall back to the default empty label if it is available.
  auto iter = encryption_key_map_.find(stream_label);
  if (iter == encryption_key_map_.end()) {
    iter = encryption_key_map_.find(kEmptyDrmLabel);
    if (iter == encryption_key_map_.end()) {
      return Status(error::NOT_FOUND,
                    "Key for '" + stream_label + "' was not found.");
    }
  }
  *key = *iter->second;
  return Status::OK;
}

Status RawKeySource::GetKey(const std::vector<uint8_t>& key_id,
                            EncryptionKey* key) {
  DCHECK(key);
  for (const auto& pair : encryption_key_map_) {
    if (pair.second->key_id == key_id) {
      *key = *pair.second;
      return Status::OK;
    }
  }
  return Status(error::INTERNAL_ERROR,
                "Key for key_id=" + base::HexEncode(&key_id[0], key_id.size()) +
                    " was not found.");
}

Status RawKeySource::GetCryptoPeriodKey(uint32_t crypto_period_index,
                                        const std::string& stream_label,
                                        EncryptionKey* key) {
  RETURN_IF_ERROR(GetKey(stream_label, key));

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

  // Clear |key->key_system_info| to prepare for update. The original
  // |key_system_info| is saved as it may still be useful later.
  std::vector<ProtectionSystemSpecificInfo> original_key_system_info;
  key->key_system_info.swap(original_key_system_info);

  EncryptionKeyMap encryption_key_map;
  encryption_key_map[stream_label].reset(new EncryptionKey(*key));
  RETURN_IF_ERROR(UpdateProtectionSystemInfo(&encryption_key_map));
  key->key_system_info = encryption_key_map[stream_label]->key_system_info;

  // It is possible that the generated |key_system_info| is empty. This happens
  // when RawKeyParams.pssh is specified. Restore the original key system info
  // in this case.
  if (key->key_system_info.empty())
    key->key_system_info.swap(original_key_system_info);

  return Status::OK;
}

std::unique_ptr<RawKeySource> RawKeySource::Create(const RawKeyParams& raw_key,
                                                   int protection_systems_flags,
                                                   FourCC protection_scheme) {
  std::vector<ProtectionSystemSpecificInfo> key_system_info;
  bool pssh_provided = false;
  if (!raw_key.pssh.empty()) {
    pssh_provided = true;
    if (!ProtectionSystemSpecificInfo::ParseBoxes(
            raw_key.pssh.data(), raw_key.pssh.size(), &key_system_info)) {
      LOG(ERROR) << "--pssh argument should be full PSSH boxes.";
      return std::unique_ptr<RawKeySource>();
    }
  }

  EncryptionKeyMap encryption_key_map;
  for (const auto& entry : raw_key.key_map) {
    const std::string& drm_label = entry.first;
    const RawKeyParams::KeyInfo& key_pair = entry.second;

    if (key_pair.key_id.size() != 16) {
      LOG(ERROR) << "Invalid key ID size '" << key_pair.key_id.size()
                 << "', must be 16 bytes.";
      return std::unique_ptr<RawKeySource>();
    }
    if (key_pair.key.size() != 16) {
      // CENC only supports AES-128, i.e. 16 bytes.
      LOG(ERROR) << "Invalid key size '" << key_pair.key.size()
                 << "', must be 16 bytes.";
      return std::unique_ptr<RawKeySource>();
    }

    std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey);
    encryption_key->key_id = key_pair.key_id;
    encryption_key->key = key_pair.key;
    encryption_key->iv = raw_key.iv;
    encryption_key->key_system_info = key_system_info;
    encryption_key_map[drm_label] = std::move(encryption_key);
  }

  // Generate common protection system if no other protection system is
  // specified.
  if (!pssh_provided && protection_systems_flags == NO_PROTECTION_SYSTEM_FLAG) {
    protection_systems_flags = COMMON_PROTECTION_SYSTEM_FLAG;
  }

  return std::unique_ptr<RawKeySource>(
      new RawKeySource(std::move(encryption_key_map), protection_systems_flags,
                       protection_scheme));
}

RawKeySource::RawKeySource()
    : KeySource(NO_PROTECTION_SYSTEM_FLAG, FOURCC_NULL) {}

RawKeySource::RawKeySource(EncryptionKeyMap&& encryption_key_map,
                           int protection_systems_flags,
                           FourCC protection_scheme)
    : KeySource(protection_systems_flags, protection_scheme),
      encryption_key_map_(std::move(encryption_key_map)) {
  UpdateProtectionSystemInfo(&encryption_key_map_);
}

}  // namespace media
}  // namespace shaka
