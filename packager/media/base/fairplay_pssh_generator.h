// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_FAIRPLAY_PSSH_GENERATOR_H_
#define PACKAGER_MEDIA_BASE_FAIRPLAY_PSSH_GENERATOR_H_

#include "packager/media/base/pssh_generator.h"

namespace shaka {
namespace media {

// Unofficial FairPlay system id extracted from
// https://forums.developer.apple.com/thread/6185.
const uint8_t kFairPlaySystemId[] = {0x29, 0x70, 0x1F, 0xE4, 0x3C, 0xC7,
                                     0x4A, 0x34, 0x8C, 0x5B, 0xAE, 0x90,
                                     0xC7, 0x43, 0x9A, 0x47};

class FairPlayPsshGenerator : public PsshGenerator {
 public:
  FairPlayPsshGenerator();
  ~FairPlayPsshGenerator() override;

  /// @name PsshGenerator implemetation overrides.
  /// @{
  bool SupportMultipleKeys() override;
  /// @}

 private:
  FairPlayPsshGenerator& operator=(const FairPlayPsshGenerator&) = delete;
  FairPlayPsshGenerator(const FairPlayPsshGenerator&) = delete;

  // PsshGenerator implemetation overrides.

  base::Optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIds(
      const std::vector<std::vector<uint8_t>>& key_ids) const override;

  base::Optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIdAndKey(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key) const override;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_FAIRPLAY_PSSH_GENERATOR_H_
