// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mpeg/adts_constants.h"

#include "packager/base/macros.h"

namespace shaka {
namespace media {

// The following conversion table is extracted from ISO 14496 Part 3 -
// Table 1.16 - Sampling Frequency Index.
const int kAdtsFrequencyTable[] = {96000, 88200, 64000, 48000, 44100,
                                   32000, 24000, 22050, 16000, 12000,
                                   11025, 8000,  7350};
const size_t kAdtsFrequencyTableSize = arraysize(kAdtsFrequencyTable);

// The following conversion table is extracted from ISO 14496 Part 3 -
// Table 1.17 - Channel Configuration.
const int kAdtsNumChannelsTable[] = {
    0, 1, 2, 3, 4, 5, 6, 8 };
const size_t kAdtsNumChannelsTableSize = arraysize(kAdtsNumChannelsTable);

}  // namespace media
}  // namespace shaka
