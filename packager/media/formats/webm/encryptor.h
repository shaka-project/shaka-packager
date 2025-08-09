// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_ENCRYPTOR_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_ENCRYPTOR_H_

#include <cstdint>
#include <vector>

#include <mkvmuxer/mkvmuxer.h>

#include <packager/status.h>

namespace shaka {
namespace media {

class MediaSample;

namespace webm {

/// Adds the encryption info with the specified key_id to the given track.
/// @return OK on success, an error status otherwise.
Status UpdateTrackForEncryption(const std::vector<uint8_t>& key_id,
                                mkvmuxer::Track* track);

/// Update the frame with signal bytes and encryption information if it is
/// encrypted.
void UpdateFrameForEncryption(MediaSample* sample);

}  // namespace webm
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_ENCRYPTOR_H_
