// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_VIDEO_UTIL_H_
#define PACKAGER_MEDIA_BASE_VIDEO_UTIL_H_

#include <cstdint>

namespace shaka {
namespace media {

// Derive pixel aspect ratio from Display Aspect Ratio and Frame Aspect Ratio.
void DerivePixelWidthHeight(uint32_t frame_width,
                            uint32_t frame_height,
                            uint32_t display_width,
                            uint32_t display_height,
                            uint32_t* pixel_width,
                            uint32_t* pixel_height);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_VIDEO_UTIL_H_
