// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Timestamps are derived directly from the encoded media file and are commonly
// known as the presentation timestamp (PTS).  Durations are a best-guess and
// are usually derived from the sample/frame rate of the media file.
//
// Due to encoding and transmission errors, it is not guaranteed that timestamps
// arrive in a monotonically increasing order nor that the next timestamp will
// be equal to the previous timestamp plus the duration.
//
// In the ideal scenario for a 25fps movie, buffers are timestamped as followed:
//
//               Buffer0      Buffer1      Buffer2      ...      BufferN
// Timestamp:        0us      40000us      80000us      ...   (N*40000)us
// Duration*:    40000us      40000us      40000us      ...       40000us
//
//  *25fps = 0.04s per frame = 40000us per frame

#ifndef MEDIA_BASE_BUFFERS_H_
#define MEDIA_BASE_BUFFERS_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// TODO(scherkus): Move the contents of this file elsewhere.

// Indicates an invalid or missing timestamp.
MEDIA_EXPORT extern inline base::TimeDelta kNoTimestamp() {
  return base::TimeDelta::FromMicroseconds(kint64min);
}

// Represents an infinite stream duration.
MEDIA_EXPORT extern inline base::TimeDelta kInfiniteDuration() {
  return base::TimeDelta::FromMicroseconds(kint64max);
}

}  // namespace media

#endif  // MEDIA_BASE_BUFFERS_H_
