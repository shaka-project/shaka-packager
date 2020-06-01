// Copyright 2020 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// AC4 audio utility functions.

#ifndef PACKAGER_MEDIA_CODECS_AC4_AUDIO_UTIL_H_
#define PACKAGER_MEDIA_CODECS_AC4_AUDIO_UTIL_H_

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <string>

namespace shaka {
namespace media {

/// Parse data from AC4Specific box and calculate AC4 channel mask value based
/// on ETSI TS 103 192-2 V1.2.1 Digital Audio Compression (AC-4)
/// Standard E.10.14.
/// @return false if there are parsing errors.
bool CalculateAC4ChannelMask(const std::vector<uint8_t>& ac4_data,
                             uint32_t& channel_mask);

/// Calculate AC4 audio channel configuration descriptor value with MPEG scheme
/// based on ETSI TS 103 192-2 V1.2.1 Digital Audio Compression (AC-4) Standard
/// G.3.2
/// @return false if there are parsing errors.
bool CalculateAC4ChannelMpegValue(const std::vector<uint8_t>& ac4_data,
                                  uint32_t& channel_mpeg_value);

/// Get AC4 codec information (bitstream_versionm, presentation_version and
/// mdcompat) based on ETSI TS 103 190-2, V1.2.1 Digital Audio Compression
/// (AC-4) Standard E.13.
/// @return false if there are parsing errors.
bool GetAc4CodecInfo(const std::vector<uint8_t>& ac4_data, uint8_t& codec_info);

/// Get AC4 Immersive stereo (IMS) information including IMS flag and source
/// content Atmos flag.
/// @return false if there are parsing errors.
bool GetAc4ImsInfo(const std::vector<uint8_t>& ac4_data, bool& ims_flag,
                   bool& src_atmos_flag);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_AC4_AUDIO_UTIL_H_
