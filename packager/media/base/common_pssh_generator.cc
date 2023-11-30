// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/common_pssh_generator.h>

#include <packager/macros/classes.h>
#include <packager/macros/compiler.h>
#include <packager/macros/logging.h>
#include <packager/media/base/protection_system_ids.h>

namespace shaka {
namespace media {
namespace {
const uint8_t kCommonSystemPsshBoxVersion = 1;
}  // namespace

CommonPsshGenerator::CommonPsshGenerator()
    : PsshGenerator(std::vector<uint8_t>(std::begin(kCommonSystemId),
                                         std::end(kCommonSystemId)),
                    kCommonSystemPsshBoxVersion) {}

CommonPsshGenerator::~CommonPsshGenerator() = default;

bool CommonPsshGenerator::SupportMultipleKeys() {
  return true;
}

std::optional<std::vector<uint8_t>>
CommonPsshGenerator::GeneratePsshDataFromKeyIdAndKey(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key) const {
  UNUSED(key_id);
  UNUSED(key);
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<std::vector<uint8_t>>
CommonPsshGenerator::GeneratePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids) const {
  UNUSED(key_ids);
  // Intentionally empty PSSH data for RawKey.
  return std::vector<uint8_t>();
}

}  // namespace media
}  // namespace shaka
