// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/fairplay_pssh_generator.h"

namespace shaka {
namespace media {
namespace {
const uint8_t kFairPlayPsshBoxVersion = 1;
}  // namespace

FairPlayPsshGenerator::FairPlayPsshGenerator()
    : PsshGenerator(std::vector<uint8_t>(std::begin(kFairPlaySystemId),
                                         std::end(kFairPlaySystemId)),
                    kFairPlayPsshBoxVersion) {}

FairPlayPsshGenerator::~FairPlayPsshGenerator() = default;

bool FairPlayPsshGenerator::SupportMultipleKeys() {
  return true;
}

base::Optional<std::vector<uint8_t>>
FairPlayPsshGenerator::GeneratePsshDataFromKeyIdAndKey(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& key) const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<std::vector<uint8_t>>
FairPlayPsshGenerator::GeneratePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids) const {
  // Intentionally empty PSSH data for FairPlay.
  return std::vector<uint8_t>();
}

}  // namespace media
}  // namespace shaka
