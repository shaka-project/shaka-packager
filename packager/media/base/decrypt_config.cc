// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/base/decrypt_config.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {

DecryptConfig::DecryptConfig(const std::vector<uint8_t>& key_id,
                             const std::vector<uint8_t>& iv,
                             const std::vector<SubsampleEntry>& subsamples)
    : DecryptConfig(key_id, iv, subsamples, FOURCC_cenc, 0, 0) {}

DecryptConfig::DecryptConfig(const std::vector<uint8_t>& key_id,
                             const std::vector<uint8_t>& iv,
                             const std::vector<SubsampleEntry>& subsamples,
                             FourCC protection_scheme,
                             uint8_t crypt_byte_block,
                             uint8_t skip_byte_block)
    : key_id_(key_id),
      iv_(iv),
      subsamples_(subsamples),
      protection_scheme_(protection_scheme),
      crypt_byte_block_(crypt_byte_block),
      skip_byte_block_(skip_byte_block) {
  CHECK_GT(key_id.size(), 0u);
}

DecryptConfig::~DecryptConfig() {}

size_t DecryptConfig::GetTotalSizeOfSubsamples() const {
  size_t size = 0;
  for (const SubsampleEntry& subsample : subsamples_)
    size += subsample.clear_bytes + subsample.cipher_bytes;
  return size;
}

}  // namespace media
}  // namespace shaka
