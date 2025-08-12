// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_PSSH_GENERATOR_UTIL_H_
#define PACKAGER_MEDIA_BASE_PSSH_GENERATOR_UTIL_H_

#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

namespace shaka {
namespace media {

std::vector<uint8_t> GenerateWidevinePsshDataFromKeyIds(
    const std::vector<std::vector<uint8_t>>& key_ids);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_PSSH_GENERATOR_UTIL_H_
