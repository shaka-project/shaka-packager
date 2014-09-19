// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decrypt_config.h"

#include "base/logging.h"

namespace edash_packager {
namespace media {

DecryptConfig::DecryptConfig(const std::vector<uint8>& key_id,
                             const std::vector<uint8>& iv,
                             const int data_offset,
                             const std::vector<SubsampleEntry>& subsamples)
    : key_id_(key_id),
      iv_(iv),
      data_offset_(data_offset),
      subsamples_(subsamples) {
  CHECK_GT(key_id.size(), 0u);
  CHECK_GE(data_offset, 0);
}

DecryptConfig::~DecryptConfig() {}

}  // namespace media
}  // namespace edash_packager
