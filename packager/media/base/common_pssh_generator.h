// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_COMMON_PSSH_GENERATOR_H_
#define PACKAGER_MEDIA_BASE_COMMON_PSSH_GENERATOR_H_

#include <cstdint>

#include <packager/media/base/pssh_generator.h>

namespace shaka {
namespace media {

class CommonPsshGenerator : public PsshGenerator {
 public:
  CommonPsshGenerator();
  ~CommonPsshGenerator() override;

  /// @name PsshGenerator implemetation overrides.
  /// @{
  bool SupportMultipleKeys() override;
  /// @}

 private:
  CommonPsshGenerator& operator=(const CommonPsshGenerator&) = delete;
  CommonPsshGenerator(const CommonPsshGenerator&) = delete;

  // PsshGenerator implemetation overrides.

  std::optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIds(
      const std::vector<std::vector<uint8_t>>& key_ids) const override;

  std::optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIdAndKey(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key) const override;
};
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_COMMON_PSSH_GENERATOR_H_
