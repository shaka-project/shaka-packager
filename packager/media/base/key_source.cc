// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/key_source.h"

#include "packager/base/logging.h"
#include "packager/media/base/common_pssh_generator.h"
#include "packager/media/base/playready_pssh_generator.h"
#include "packager/media/base/protection_system_ids.h"
#include "packager/media/base/widevine_pssh_generator.h"
#include "packager/status_macros.h"

namespace shaka {
namespace media {

KeySource::KeySource(int protection_systems_flags, FourCC protection_scheme) {
  if (protection_systems_flags & COMMON_PROTECTION_SYSTEM_FLAG) {
    pssh_generators_.emplace_back(new CommonPsshGenerator());
  }

  if (protection_systems_flags & PLAYREADY_PROTECTION_SYSTEM_FLAG) {
    pssh_generators_.emplace_back(new PlayReadyPsshGenerator());
  }

  if (protection_systems_flags & WIDEVINE_PROTECTION_SYSTEM_FLAG) {
    pssh_generators_.emplace_back(new WidevinePsshGenerator(protection_scheme));
  }

  if (protection_systems_flags & FAIRPLAY_PROTECTION_SYSTEM_FLAG) {
    no_pssh_systems_.emplace_back(std::begin(kFairPlaySystemId),
                                  std::end(kFairPlaySystemId));
  }
  // We only support Marlin Adaptive Streaming Specification – Simple Profile
  // with Implicit Content ID Mapping, which does not need a PSSH. Marlin
  // specific PSSH with Explicit Content ID Mapping is not generated.
  if (protection_systems_flags & MARLIN_PROTECTION_SYSTEM_FLAG) {
    no_pssh_systems_.emplace_back(std::begin(kMarlinSystemId),
                                  std::end(kMarlinSystemId));
  }
}

KeySource::~KeySource() = default;

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
      RETURN_IF_ERROR(pssh_generator->GeneratePsshFromKeyIds(key_ids, &info));
      for (const EncryptionKeyMap::value_type& pair : *encryption_key_map) {
        pair.second->key_system_info.push_back(info);
      }
    } else {
      for (const EncryptionKeyMap::value_type& pair : *encryption_key_map) {
        ProtectionSystemSpecificInfo info;
        RETURN_IF_ERROR(pssh_generator->GeneratePsshFromKeyIdAndKey(
            pair.second->key_id, pair.second->key, &info));
        pair.second->key_system_info.push_back(info);
      }
    }
  }

  for (const auto& no_pssh_system : no_pssh_systems_) {
    ProtectionSystemSpecificInfo info;
    info.system_id = no_pssh_system;
    for (const EncryptionKeyMap::value_type& pair : *encryption_key_map) {
      pair.second->key_system_info.push_back(info);
    }
  }

  return Status::OK;
}

}  // namespace media
}  // namespace shaka
