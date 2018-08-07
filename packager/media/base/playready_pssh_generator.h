// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_PLAYREADY_PSSH_GENERATOR_H_
#define MEDIA_BASE_PLAYREADY_PSSH_GENERATOR_H_

#include "packager/media/base/pssh_generator.h"

namespace shaka {
namespace media {

// SystemID defined for PlayReady Drm.
const uint8_t kPlayReadySystemId[] = {0x9a, 0x04, 0xf0, 0x79, 0x98, 0x40,
                                      0x42, 0x86, 0xab, 0x92, 0xe6, 0x5b,
                                      0xe0, 0x88, 0x5f, 0x95};

class PlayReadyPsshGenerator : public PsshGenerator {
 public:
  PlayReadyPsshGenerator();
  ~PlayReadyPsshGenerator() override;

  /// @name PsshGenerator implemetation overrides.
  /// @{
  bool SupportMultipleKeys() override;
  /// @}

 private:
  PlayReadyPsshGenerator& operator=(const PlayReadyPsshGenerator&) = delete;
  PlayReadyPsshGenerator(const PlayReadyPsshGenerator&) = delete;

  // PsshGenerator implemetation overrides.

  base::Optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIds(
      const std::vector<std::vector<uint8_t>>& key_ids) const override;

  base::Optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIdAndKey(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key) const override;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_PLAYREADY_PSSH_GENERATOR_H_
