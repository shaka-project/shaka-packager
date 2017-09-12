// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/raw_key_pssh_generator.h"

#include "packager/media/base/raw_key_source.h"

namespace shaka {
namespace media {

RawKeyPsshGenerator::RawKeyPsshGenerator()
    : system_id_(std::begin(kCommonSystemId), std::end(kCommonSystemId)) {}

RawKeyPsshGenerator::~RawKeyPsshGenerator() {}

bool RawKeyPsshGenerator::SupportMultipleKeys() {
  return true;
}

uint8_t RawKeyPsshGenerator::PsshBoxVersion() const {
  return 1;
}

const std::vector<uint8_t>& RawKeyPsshGenerator::SystemId() const {
  return system_id_;
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
