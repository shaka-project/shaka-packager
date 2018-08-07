// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/widevine_pssh_generator.h"

#include "packager/media/base/pssh_generator_util.h"

namespace shaka {
namespace media {
namespace {
// Use version 0 for backward compatibility.
const uint8_t kWidevinePsshBoxVersion = 0;
}  // namespace

WidevinePsshGenerator::WidevinePsshGenerator()
    : PsshGenerator(std::vector<uint8_t>(std::begin(kWidevineSystemId),
                                         std::end(kWidevineSystemId)),
                    kWidevinePsshBoxVersion) {}

WidevinePsshGenerator::~WidevinePsshGenerator() {}

bool WidevinePsshGenerator::SupportMultipleKeys() {
  return true;
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
