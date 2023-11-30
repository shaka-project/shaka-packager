// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Enhanced AC3 audio utility functions.

#ifndef PACKAGER_MEDIA_CODECS_EC3_AUDIO_UTIL_H_
#define PACKAGER_MEDIA_CODECS_EC3_AUDIO_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace shaka {
namespace media {

/// Parse data from EC3Specific box and calculate EC3 channel map value based on
/// ETSI TS 102 366 V1.3.1 Digital Audio Compression (AC-3, Enhanced AC-3)
/// Standard E.1.3.1.8.
bool CalculateEC3ChannelMap(const std::vector<uint8_t>& ec3_data,
                            uint32_t* channel_map);

/// Parse data from EC3Specific box and calculate number of channels.
/// @return The number of channels associated with the input ec3 data on
///         success; otherwise 0 is returned.
size_t GetEc3NumChannels(const std::vector<uint8_t>& ec3_data);

/// Parse data from EC3Specific box, calculate EC3 channel map and then
/// obtain channel configuration descriptor value with MPEG scheme based on
/// ETSI TS 102 366 V1.4.1 Digital Audio Compression (AC-3, Enhanced AC-3)
/// Standard, Table I.1.1.
bool CalculateEC3ChannelMPEGValue(const std::vector<uint8_t>& ec3_data,
                                  uint32_t* ec3_channel_mpeg_value);

/// Parse data from EC3Specific box and obtain Dolby Digital Plus JOC
/// decoding complexity based on ETSI TS 103 420 v1.2.1 Backwards-compatible
/// object audio carriage using Enhanced AC-3 Standard chapter C.3.1.
bool GetEc3JocComplexity(const std::vector<uint8_t>& ec3_data,
                         uint32_t* ec3_joc_complexity);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_EC3_AUDIO_UTIL_H_
