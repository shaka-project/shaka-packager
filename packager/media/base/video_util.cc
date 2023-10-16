// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/video_util.h>

#include <limits>

namespace {

uint64_t CalculateGCD(uint64_t a, uint64_t b) {
  while (b != 0) {
    uint64_t temp = a;
    a = b;
    b = temp % b;
  }
  return a;
}

void ReducePixelWidthHeight(uint64_t* pixel_width, uint64_t* pixel_height) {
  if (*pixel_width == 0 || *pixel_height == 0)
    return;
  const uint64_t kMaxUint32 = std::numeric_limits<uint32_t>::max();
  while (true) {
    uint64_t gcd = CalculateGCD(*pixel_width, *pixel_height);
    *pixel_width /= gcd;
    *pixel_height /= gcd;
    // Both width and height needs to be 32 bit or less.
    if (*pixel_width <= kMaxUint32 && *pixel_height <= kMaxUint32)
      break;
    *pixel_width >>= 1;
    *pixel_height >>= 1;
  }
}

}  // namespace

namespace shaka {
namespace media {

void DerivePixelWidthHeight(uint32_t frame_width,
                            uint32_t frame_height,
                            uint32_t display_width,
                            uint32_t display_height,
                            uint32_t* pixel_width,
                            uint32_t* pixel_height) {
  //   DAR = PAR * FAR => PAR = DAR / FAR.
  //   Thus:
  //     pixel_width             display_width            frame_width
  //     -----------      =      -------------      /     -----------
  //     pixel_height            display_height           frame_height
  //   So:
  //     pixel_width             display_width  x  frame_height
  //     -----------      =      ------------------------------
  //     pixel_height            display_height x  frame_width
  uint64_t pixel_width_unreduced =
      static_cast<uint64_t>(display_width) * frame_height;
  uint64_t pixel_height_unreduced =
      static_cast<uint64_t>(display_height) * frame_width;
  ReducePixelWidthHeight(&pixel_width_unreduced, &pixel_height_unreduced);
  *pixel_width = pixel_width_unreduced;
  *pixel_height = pixel_height_unreduced;
}

}  // namespace media
}  // namespace shaka
