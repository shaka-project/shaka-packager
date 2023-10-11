// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// AC4 audio utility functions.

#ifndef PACKAGER_MEDIA_CODECS_AC4_AUDIO_UTIL_H_
#define PACKAGER_MEDIA_CODECS_AC4_AUDIO_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace shaka {
namespace media {

/// Parse data from AC4Specific box and calculate AC4 channel mask value based
/// on ETSI TS 103 192-2 V1.2.1 Digital Audio Compression (AC-4) Standard;
/// Part 2: Immersive and personalized E.10.14.
/// @return false if there are parsing errors.
bool CalculateAC4ChannelMask(const std::vector<uint8_t>& ac4_data,
                             uint32_t* ac4_channel_mask);

/// Parse data from AC4Specific box, calculate AC4 channel mask and then
/// obtain channel configuration descriptor value with MPEG scheme based on
/// ETSI TS 103 192-2 V1.2.1 Digital Audio Compression (AC-4) Standard;
/// Part 2: Immersive and personalized G.3.2.
/// @return false if there are parsing errors.
bool CalculateAC4ChannelMPEGValue(const std::vector<uint8_t>& ac4_data,
                                  uint32_t* ac4_channel_mpeg_value);

/// Parse data from AC4Specific box and obtain AC4 codec information
/// (bitstream version, presentation version and mdcompat) based on ETSI TS
/// 103 190-2, V1.2.1 Digital Audio Compression (AC-4) Standard;
/// Part 2: Immersive and personalized E.13.
/// @return false if there are parsing errors.
bool GetAc4CodecInfo(const std::vector<uint8_t>& ac4_data,
                     uint8_t* ac4_codec_info);

/// Parse data from AC4Specific box and obtain AC4 Immersive stereo (IMS) flag
/// and Channel-base audio (CBI) flag.
/// @return false if there are parsing errors.
bool GetAc4ImmersiveInfo(const std::vector<uint8_t>& ac4_data,
                         bool* ac4_ims_flag,
                         bool* ac4_cbi_flag);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_AC4_AUDIO_UTIL_H_
