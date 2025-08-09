// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_WIDEVINE_PSSH_GENERATOR_H_
#define MEDIA_BASE_WIDEVINE_PSSH_GENERATOR_H_

#include <cstdint>

#include <packager/media/base/fourccs.h>
#include <packager/media/base/pssh_generator.h>

namespace shaka {
namespace media {

class WidevinePsshGenerator : public PsshGenerator {
 public:
  explicit WidevinePsshGenerator(FourCC protection_scheme);
  ~WidevinePsshGenerator() override;

  /// @name PsshGenerator implemetation overrides.
  /// @{
  bool SupportMultipleKeys() override;
  /// @}

 private:
  WidevinePsshGenerator& operator=(const WidevinePsshGenerator&) = delete;
  WidevinePsshGenerator(const WidevinePsshGenerator&) = delete;

  // PsshGenerator implemetation overrides.

  std::optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIds(
      const std::vector<std::vector<uint8_t>>& key_ids) const override;

  std::optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIdAndKey(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key) const override;

  FourCC protection_scheme_ = FOURCC_NULL;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_WIDEVINE_PSSH_GENERATOR_H_
