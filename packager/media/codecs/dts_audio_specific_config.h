// Copyright (c) 2023 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_CODECS_DTS_AUDIO_SPECIFIC_CONFIG_H_
#define PACKAGER_MEDIA_CODECS_DTS_AUDIO_SPECIFIC_CONFIG_H_

#include <cstdint>
#include <vector>

#include <stddef.h>
#include <stdint.h>

namespace shaka {
namespace media {

class BitReader;

bool GetDTSXChannelMask(const std::vector<uint8_t>& udts, uint32_t& mask);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_DTS_AUDIO_SPECIFIC_CONFIG_H_
