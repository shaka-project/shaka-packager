// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/key_source.h"

#include "packager/base/logging.h"
#include "packager/media/base/fairplay_pssh_generator.h"
#include "packager/media/base/playready_pssh_generator.h"
#include "packager/media/base/raw_key_pssh_generator.h"
#include "packager/media/base/widevine_pssh_generator.h"

namespace shaka {
namespace media {

EncryptionKey::EncryptionKey() {}

EncryptionKey::~EncryptionKey() {}

KeySource::KeySource(int protection_systems_flags, FourCC protection_scheme) {
  if (protection_systems_flags & COMMON_PROTECTION_SYSTEM_FLAG) {
    pssh_generators_.emplace_back(new RawKeyPsshGenerator());
  }

  if (protection_systems_flags & PLAYREADY_PROTECTION_SYSTEM_FLAG) {
    pssh_generators_.emplace_back(new PlayReadyPsshGenerator());
  }

  if (protection_systems_flags & WIDEVINE_PROTECTION_SYSTEM_FLAG) {
    pssh_generators_.emplace_back(new WidevinePsshGenerator(protection_scheme));
  }

  if (protection_systems_flags & FAIRPLAY_PROTECTION_SYSTEM_FLAG) {
    pssh_generators_.emplace_back(new FairPlayPsshGenerator());
  }
}

KeySource::~KeySource() {}

Status KeySource::UpdateProtectionSystemInfo(
    EncryptionKeyMap* encryption_key_map) {
  for (const auto& pssh_generator : pssh_generators_) {
    const bool support_multiple_keys = pssh_generator->SupportMultipleKeys();
    if (support_multiple_keys) {
      ProtectionSystemSpecificInfo info;
      std::vector<std::vector<uint8_t>> key_ids;
      for (const EncryptionKeyMap::value_type& pair : *encryption_key_map) {
        key_ids.push_back(pair.second->key_id);
      }
      Status status = pssh_generator->GeneratePsshFromKeyIds(key_ids, &info);
      if (!status.ok()) {
        return status;
      }
      for (const EncryptionKeyMap::value_type& pair : *encryption_key_map) {
        pair.second->key_system_info.push_back(info);
      }
    } else {
      for (const EncryptionKeyMap::value_type& pair : *encryption_key_map) {
        ProtectionSystemSpecificInfo info;
        Status status = pssh_generator->GeneratePsshFromKeyIdAndKey(
            pair.second->key_id, pair.second->key, &info);
        if (!status.ok()) {
          return status;
        }
        pair.second->key_system_info.push_back(info);
      }
    }
  }

  return Status::OK;
}

}  // namespace media
}  // namespace shaka
