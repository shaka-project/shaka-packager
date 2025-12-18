// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_HLS_AUDIO_UTIL_H_
#define PACKAGER_MEDIA_CODECS_HLS_AUDIO_UTIL_H_

#include <cstdint>

#include <packager/media/base/stream_info.h>

namespace shaka {
namespace media {

class BufferWriter;

/// Write "Audio Setup Information" according to the specification at
/// https://goo.gl/X35ZRE MPEG-2 Stream Encryption Format for HTTP Live
/// Streaming 2.3.2.
/// @return true on success.
bool WriteAudioSetupInformation(Codec codec,
                                const uint8_t* audio_specific_config,
                                size_t audio_specific_config_size,
                                BufferWriter* audio_setup_information);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_HLS_AUDIO_UTIL_H_
