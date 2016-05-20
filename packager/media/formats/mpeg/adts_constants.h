// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
#define MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_

#include <stddef.h>

namespace shaka {
namespace media {

enum {
  kAdtsHeaderMinSize = 7,
  kSamplesPerAACFrame = 1024,
};

extern const int kAdtsFrequencyTable[];
extern const size_t kAdtsFrequencyTableSize;

extern const int kAdtsNumChannelsTable[];
extern const size_t kAdtsNumChannelsTableSize;

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
