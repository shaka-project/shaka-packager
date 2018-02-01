// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_KEY_FRAME_INFO_H_
#define PACKAGER_MEDIA_FORMATS_MP4_KEY_FRAME_INFO_H_

#include <cstdint>

namespace shaka {
namespace media {
namespace mp4 {

/// Tracks key frame information.
struct KeyFrameInfo {
  uint64_t timestamp;
  uint64_t start_byte_offset;
  uint64_t size;
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_KEY_FRAME_INFO_H_
