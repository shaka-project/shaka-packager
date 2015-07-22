// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

/// This file contains helper functions and enums for MpdNotifier
/// implementations.

#ifndef MPD_BASE_MPD_NOTIFIER_UTIL_H_
#define MPD_BASE_MPD_NOTIFIER_UTIL_H_

#include <string>

#include "packager/base/base64.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_builder.h"

namespace edash_packager{

enum ContentType {
  kContentTypeUnknown,
  kContentTypeVideo,
  kContentTypeAudio,
  kContentTypeText
};

/// Converts hex data to UUID format. Hex data must be size 16.
/// @param data input hex data.
/// @param uuid_format is the UUID format of the input.
bool HexToUUID(const std::string& data, std::string* uuid_format);

/// Outputs MPD to @a output_path.
/// @param output_path is the path to the MPD output location.
/// @param mpd_builder is the MPD builder instance.
bool WriteMpdToFile(const std::string& output_path, MpdBuilder* mpd_builder);

/// Determines the content type of |media_info|.
/// @param media_info is the information about the media.
/// @return content type of the @a media_info.
ContentType GetContentType(const MediaInfo& media_info);

/// Adds <ContentProtection> elements specified by @a media_info to
/// @a adaptation_set.
/// Note that this will add the elements as direct chlidren of AdaptationSet.
/// @param media_info may or may not have protected_content field.
/// @param adaptation_set is the parent element that owns the ContentProtection
///        elements.
void AddContentProtectionElements(const MediaInfo& media_info,
                                  AdaptationSet* adaptation_set);

/// Adds <ContentProtection> elements specified by @a media_info to
/// @a representation.
/// @param media_info may or may not have protected_content field.
/// @param representation is the parent element that owns the ContentProtection
///        elements.
void AddContentProtectionElements(const MediaInfo& media_info,
                                  Representation* representation);


}  // namespace edash_packager

#endif  // MPD_BASE_MPD_NOTIFIER_UTIL_H_
