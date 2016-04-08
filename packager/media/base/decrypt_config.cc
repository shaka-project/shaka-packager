// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/base/decrypt_config.h"

#include "packager/base/logging.h"

namespace edash_packager {
namespace media {

DecryptConfig::DecryptConfig(const std::vector<uint8_t>& key_id,
                             const std::vector<uint8_t>& iv,
                             const std::vector<SubsampleEntry>& subsamples)
    : DecryptConfig(key_id, iv, subsamples, FOURCC_cenc) {}

DecryptConfig::DecryptConfig(const std::vector<uint8_t>& key_id,
                             const std::vector<uint8_t>& iv,
                             const std::vector<SubsampleEntry>& subsamples,
                             FourCC protection_scheme)
    : key_id_(key_id),
      iv_(iv),
      subsamples_(subsamples),
      protection_scheme_(protection_scheme) {
  CHECK_GT(key_id.size(), 0u);
}

DecryptConfig::~DecryptConfig() {}

}  // namespace media
}  // namespace edash_packager
