// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// AC3 audio utility functions.

#ifndef PACKAGER_MEDIA_CODECS_AC3_AUDIO_UTIL_H_
#define PACKAGER_MEDIA_CODECS_AC3_AUDIO_UTIL_H_

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace shaka {
namespace media {

/// Parse data from AC3Specific box and calculate number of channels.
/// @return The number of channels associated with the input ac3 data on
///         success; otherwise 0 is returned.
size_t GetAc3NumChannels(const std::vector<uint8_t>& ac3_data);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_AC3_AUDIO_UTIL_H_
