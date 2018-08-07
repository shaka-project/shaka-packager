// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/raw_key_pssh_generator.h"

namespace shaka {
namespace media {
namespace {
const uint8_t kCommonSystemPsshBoxVersion = 1;
}  // namespace

RawKeyPsshGenerator::RawKeyPsshGenerator()
    : PsshGenerator(std::vector<uint8_t>(std::begin(kCommonSystemId),
                                         std::end(kCommonSystemId)),
                    kCommonSystemPsshBoxVersion) {}

RawKeyPsshGenerator::~RawKeyPsshGenerator() = default;

bool RawKeyPsshGenerator::SupportMultipleKeys() {
  return true;
}

base::Optional<std::vector<uint8_t>>
RawKeyPsshGenerator::GeneratePsshDataFromKeyIdAndKey(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<std::vector<uint8_t>>
RawKeyPsshGenerator::GeneratePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids) const {
  // Intentionally empty PSSH data for RawKey.
  return std::vector<uint8_t>();
}

}  // namespace media
}  // namespace shaka
