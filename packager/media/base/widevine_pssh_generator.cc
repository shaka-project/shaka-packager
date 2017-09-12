// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/widevine_pssh_generator.h"

#include "packager/media/base/pssh_generator_util.h"
#include "packager/media/base/widevine_key_source.h"

namespace shaka {
namespace media {

WidevinePsshGenerator::WidevinePsshGenerator()
    : system_id_(std::begin(kWidevineSystemId), std::end(kWidevineSystemId)) {}

WidevinePsshGenerator::~WidevinePsshGenerator() {}

bool WidevinePsshGenerator::SupportMultipleKeys() {
  return true;
}

uint8_t WidevinePsshGenerator::PsshBoxVersion() const {
  // This is for backward compatibility.
  return 0;
}

const std::vector<uint8_t>& WidevinePsshGenerator::SystemId() const {
  return system_id_;
}

base::Optional<std::vector<uint8_t>>
WidevinePsshGenerator::GeneratePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids) const {
  return GenerateWidevinePsshDataFromKeyIds(key_ids);
}

base::Optional<std::vector<uint8_t>>
WidevinePsshGenerator::GeneratePsshDataFromKeyIdAndKey(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}
}  // namespace media
}  // namespace shaka
