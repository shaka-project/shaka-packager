// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

/// This file contains helper functions and enums for MpdNotifier
/// implementations.

#ifndef MPD_BASE_MPD_NOTIFIER_UTIL_H_
#define MPD_BASE_MPD_NOTIFIER_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include <absl/strings/escaping.h>

#include <packager/mpd/base/media_info.pb.h>
#include <packager/mpd/base/mpd_builder.h>

namespace shaka{

enum ContentType {
  kContentTypeUnknown,
  kContentTypeVideo,
  kContentTypeAudio,
  kContentTypeText
};

/// Outputs MPD to @a output_path.
/// @param output_path is the path to the MPD output location.
/// @param mpd_builder is the MPD builder instance.
bool WriteMpdToFile(const std::string& output_path, MpdBuilder* mpd_builder);

/// Determines the content type of |media_info|.
/// @param media_info is the information about the media.
/// @return content type of the @a media_info.
ContentType GetContentType(const MediaInfo& media_info);

/// Converts uint8 vector into base64 encoded string.
std::string Uint8VectorToBase64(const std::vector<uint8_t>& input);

}  // namespace shaka

#endif  // MPD_BASE_MPD_NOTIFIER_UTIL_H_
