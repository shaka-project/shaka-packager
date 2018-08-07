// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/pssh_generator.h"

namespace shaka {
namespace media {
namespace {

std::vector<uint8_t> CreatePsshBox(
    const std::vector<uint8_t>& system_id,
    uint8_t version,
    const std::vector<std::vector<uint8_t>>& key_ids,
    const std::vector<uint8_t>& pssh_data) {
  PsshBoxBuilder pssh_builder;
  pssh_builder.set_pssh_data(pssh_data);
  for (const std::vector<uint8_t>& key_id : key_ids) {
    pssh_builder.add_key_id(key_id);
  }
  pssh_builder.set_pssh_box_version(version);
  pssh_builder.set_system_id(system_id.data(), system_id.size());

  return pssh_builder.CreateBox();
}

}  // namespace

PsshGenerator::PsshGenerator(const std::vector<uint8_t>& system_id,
                             uint8_t box_version)
    : system_id_(system_id), box_version_(box_version) {}

PsshGenerator::~PsshGenerator() = default;

Status PsshGenerator::GeneratePsshFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids,
    ProtectionSystemSpecificInfo* info) const {
  base::Optional<std::vector<uint8_t>> pssh_data =
      GeneratePsshDataFromKeyIds(key_ids);
  if (!pssh_data) {
    return Status(error::ENCRYPTION_FAILURE,
                  "Fail to generate PSSH data from multiple Key IDs.");
  }
  info->system_id = system_id_;
  info->psshs =
      CreatePsshBox(system_id_, box_version_, key_ids, pssh_data.value());
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
  info->system_id = system_id_;
  info->psshs =
      CreatePsshBox(system_id_, box_version_, {key_id}, pssh_data.value());
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
