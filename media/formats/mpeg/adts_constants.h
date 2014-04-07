// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
#define MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_

#include <stddef.h>

namespace media {

enum {
  kADTSHeaderMinSize = 7,
  kSamplesPerAACFrame = 1024,
};

extern const int kADTSFrequencyTable[];
extern const size_t kADTSFrequencyTableSize;

extern const int kADTSChannelLayoutTable[];
extern const size_t kADTSChannelLayoutTableSize;

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_ADTS_CONSTANTS_H_
