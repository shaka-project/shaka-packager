// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/pssh_generator.h"

namespace shaka {
namespace media {

PsshGenerator::PsshGenerator() {}

PsshGenerator::~PsshGenerator() {}

Status PsshGenerator::GeneratePsshFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids,
    ProtectionSystemSpecificInfo* info) const {
  base::Optional<std::vector<uint8_t>> pssh_data =
      GeneratePsshDataFromKeyIds(key_ids);
  if (!pssh_data) {
    return Status(error::ENCRYPTION_FAILURE,
                  "Fail to generate PSSH data from multiple Key IDs.");
  }
  info->set_pssh_data(pssh_data.value());

  info->clear_key_ids();
  for (const auto& key_id : key_ids) {
    info->add_key_id(key_id);
  }

  info->set_pssh_box_version(PsshBoxVersion());
  const std::vector<uint8_t>& system_id = SystemId();
  info->set_system_id(system_id.data(), system_id.size());

  return Status::OK;
}

Status PsshGenerator::GeneratePsshFromKeyIdAndKey(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key,
    ProtectionSystemSpecificInfo* info) const {
  base::Optional<std::vector<uint8_t>> pssh_data =
      GeneratePsshDataFromKeyIdAndKey(key_id, key);
  if (!pssh_data) {
    return Status(error::ENCRYPTION_FAILURE,
                  "Fail to generate PSSH data from Key ID and Key.");
  }
  info->set_pssh_data(pssh_data.value());

  info->clear_key_ids();
  info->add_key_id(key_id);
  info->set_pssh_box_version(PsshBoxVersion());
  const std::vector<uint8_t>& system_id = SystemId();
  info->set_system_id(system_id.data(), system_id.size());

  return Status::OK;
}

}  // namespace media
}  // namespace shaka
