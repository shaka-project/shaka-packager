// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd


#ifndef MEDIA_BASE_TIMESTAMP_H_
#define MEDIA_BASE_TIMESTAMP_H_

#include "base/basictypes.h"

namespace edash_packager {
namespace media {

const int64_t kNoTimestamp = kint64min;
const int64_t kInfiniteDuration = kint64max;

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_TIMESTAMP_H_
