// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_ENCRYPTION_CONFIG_H_
#define PACKAGER_MEDIA_BASE_ENCRYPTION_CONFIG_H_

#include <cstdint>

#include <packager/media/base/fourccs.h>
#include <packager/media/base/protection_system_specific_info.h>

namespace shaka {
namespace media {

struct EncryptionConfig {
  FourCC protection_scheme = FOURCC_cenc;
  uint8_t crypt_byte_block = 0;
  uint8_t skip_byte_block = 0;
  uint8_t per_sample_iv_size = 0;
  std::vector<uint8_t> constant_iv;
  std::vector<uint8_t> key_id;
  std::vector<ProtectionSystemSpecificInfo> key_system_info;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_ENCRYPTION_CONFIG_H_
