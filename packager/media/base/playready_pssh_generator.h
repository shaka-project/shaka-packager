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
