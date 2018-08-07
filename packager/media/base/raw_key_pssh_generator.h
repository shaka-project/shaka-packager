// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_RAW_KEY_PSSH_GENERATOR_H_
#define MEDIA_BASE_RAW_KEY_PSSH_GENERATOR_H_

#include "packager/media/base/pssh_generator.h"

namespace shaka {
namespace media {

// Common SystemID defined by EME, which requires Key System implementations
// supporting ISO Common Encryption to support this SystemID and format.
// https://goo.gl/kUv2Xd
const uint8_t kCommonSystemId[] = {0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2,
                                   0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e,
                                   0x52, 0xe2, 0xfb, 0x4b};

class RawKeyPsshGenerator : public PsshGenerator {
 public:
  RawKeyPsshGenerator();
  ~RawKeyPsshGenerator() override;

  /// @name PsshGenerator implemetation overrides.
  /// @{
  bool SupportMultipleKeys() override;
  /// @}

 private:
  RawKeyPsshGenerator& operator=(const RawKeyPsshGenerator&) = delete;
  RawKeyPsshGenerator(const RawKeyPsshGenerator&) = delete;

  // PsshGenerator implemetation overrides.

  base::Optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIds(
      const std::vector<std::vector<uint8_t>>& key_ids) const override;

  base::Optional<std::vector<uint8_t>> GeneratePsshDataFromKeyIdAndKey(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key) const override;
};
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_RAW_KEY_PSSH_GENERATOR_H_
