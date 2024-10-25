// Copyright 2024 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// IAMF audio utility functions.

#ifndef PACKAGER_MEDIA_CODECS_IAMF_AUDIO_UTIL_H_
#define PACKAGER_MEDIA_CODECS_IAMF_AUDIO_UTIL_H_

#include <cstdint>
#include <vector>

namespace shaka {
namespace media {

/// Parse data from IAMFSpecific box and obtain the profile and codec
/// information needed to construct its Codec String (Section 6.4).
/// @return false if there are parsing errors.
bool GetIamfCodecStringInfo(const std::vector<uint8_t>& iamf_data,
                            uint8_t& codec_string_info);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_IAMF_AUDIO_UTIL_H_
