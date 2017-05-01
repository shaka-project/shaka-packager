// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Event handler for events fired by Muxer.

#ifndef PACKAGER_MEDIA_BASE_RANGE_H_
#define PACKAGER_MEDIA_BASE_RANGE_H_

#include <stdint.h>

namespace shaka {
namespace media {

/// Structure for specifying a range.
/// The start and end are inclusive which is equivalent to [start, end].
struct Range {
  uint64_t start;
  uint64_t end;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_RANGE_H_
