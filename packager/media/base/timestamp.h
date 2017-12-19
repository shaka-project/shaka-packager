// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd


#ifndef PACKAGER_MEDIA_BASE_TIMESTAMP_H_
#define PACKAGER_MEDIA_BASE_TIMESTAMP_H_

#include <stdint.h>

#include <limits>

namespace shaka {
namespace media {

const int64_t kNoTimestamp = std::numeric_limits<int64_t>::min();
const int64_t kInfiniteDuration = std::numeric_limits<int64_t>::max();

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TIMESTAMP_H_
