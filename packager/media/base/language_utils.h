// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Funtions used by MpdBuilder class to generate an MPD file.

#ifndef PACKAGER_MEDIA_BASE_LANGUAGE_UTILS_H_
#define PACKAGER_MEDIA_BASE_LANGUAGE_UTILS_H_

#include <string>

namespace shaka {

class MediaInfo;

/// Convert a language tag to its shortest form, as required by RFC 5646
/// indicated in the MPD and HLS specs.  Assumes the input is a valid ISO-639-2
/// or ISO-639-1 language tag, or an empty string.  Regions and variants are
/// preserved in the conversion.
std::string LanguageToShortestForm(const std::string& language);

/// Convert a language tag to a 3-letter ISO-639-2 code, as required by the ISO
/// BMFF spec.  The input is assumed to be a valid ISO-639-2 or ISO-639-1
/// language code.  Regions and variants are not supported.
std::string LanguageToISO_639_2(const std::string& language);

}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_LANGUAGE_UTILS_H_
