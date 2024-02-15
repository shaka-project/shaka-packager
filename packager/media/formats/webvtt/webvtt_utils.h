// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_UTILS_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_UTILS_H_

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <string_view>

#include <packager/media/base/text_sample.h>
#include <packager/media/base/text_stream_info.h>

namespace shaka {
namespace media {

// Parse a timestamp into milliseconds using the two patterns defined by WebVtt:
//  LONG  : ##:##:##.### (long can have 2 or more hour digits)
//  SHORT :    ##:##:###
bool WebVttTimestampToMs(const std::string_view& source, int64_t* out);

// Create a long form timestamp encoded as a string.
std::string MsToWebVttTimestamp(uint64_t ms);

/// Converts the given text settings to a WebVTT settings string.
std::string WebVttSettingsToString(const TextSettings& settings);

/// Converts the given TextFragment to a WebVTT cue body string.
std::string WebVttFragmentToString(const TextFragment& fragment);

/// Converts the common fields in the stream into WebVTT text.  This pulls out
/// the REGION and STYLE blocks.
std::string WebVttGetPreamble(const TextStreamInfo& stream_info);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_UTILS_H_
