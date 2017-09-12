// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_WIDEVINE_PSSH_GENERATOR_H_
#define MEDIA_BASE_WIDEVINE_PSSH_GENERATOR_H_

#include <vector>

#include "packager/media/base/key_source.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace shaka {
namespace media {

const uint8_t kWidevineSystemId[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6,
                                     0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc,
                                     0xd5, 0x1d, 0x21, 0xed};
class WidevineKeySource;

class WidevinePsshGenerator : public PsshGenerator {
 public:
  WidevinePsshGenerator();
  ~WidevinePsshGenerator() override;

  /// @name PsshGenerator implemetation overrides.
  /// @{
  bool SupportMultipleKeys() override;
  /// @}

 private:
  WidevinePsshGenerator& operator=(const WidevinePsshGenerator&) = delete;
  WidevinePsshGenerator(const WidevinePsshGenerator&) = delete;

  // PsshGenerator implemetation overrides.
  uint8_t PsshBoxVersion() const override;

  const std::vector<uint8_t>& SystemId() const override;

  base::Optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIds(
      const std::vector<std::vector<uint8_t>>& key_ids) const override;

  base::Optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIdAndKey(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key) const override;

  std::vector<uint8_t> system_id_;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_WIDEVINE_PSSH_GENERATOR_H_
