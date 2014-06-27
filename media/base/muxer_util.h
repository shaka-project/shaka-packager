// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Muxer utility functions.

#ifndef MEDIA_BASE_MUXER_UTIL_H_
#define MEDIA_BASE_MUXER_UTIL_H_

#include <string>

#include "base/basictypes.h"

namespace media {

/// Validates the segment template against segment URL construction rule
/// specified in ISO/IEC 23009-1:2012 5.3.9.4.4.
/// @param segment_template is the template to be validated.
/// @return true if the segment template complies with
//          ISO/IEC 23009-1:2012 5.3.9.4.4, false otherwise.
bool ValidateSegmentTemplate(const std::string& segment_template);

/// Build the segment name from provided input.
/// @param segment_template is the segment template pattern, which should
///        comply with ISO/IEC 23009-1:2012 5.3.9.4.4.
/// @param segment_start_time specifies the segment start time.
/// @param segment_index specifies the segment index.
/// @param bandwidth represents the bit rate, in bits/sec, of the stream.
/// @return The segment name with identifier substituted.
std::string GetSegmentName(const std::string& segment_template,
                           uint64 segment_start_time,
                           uint32 segment_index,
                           uint32 bandwidth);

}  // namespace media

#endif  // MEDIA_BASE_MUXER_UTIL_H_
