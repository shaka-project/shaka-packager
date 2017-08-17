// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_TIMESTAMP_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_TIMESTAMP_H_

#include <stdint.h>

#include <string>

#include "packager/base/strings/string_piece.h"

namespace shaka {
namespace media {
// Parse a timestamp into milliseconds using the two patterns defined by WebVtt:
//  LONG  : ##:##:##.### (long can have 2 or more hour digits)
//  SHORT :    ##:##:###
bool WebVttTimestampToMs(const base::StringPiece& source, uint64_t* out);

// Create a long form timestamp encoded as a string.
std::string MsToWebVttTimestamp(uint64_t ms);
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_TIMESTAMP_H_
