// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/key_source.h"

#include "packager/base/logging.h"

namespace shaka {
namespace media {

EncryptionKey::EncryptionKey() {}
EncryptionKey::~EncryptionKey() {}

KeySource::~KeySource() {}

KeySource::TrackType KeySource::GetTrackTypeFromString(
    const std::string& track_type_string) {
  if (track_type_string == "SD")
    return TRACK_TYPE_SD;
  if (track_type_string == "HD")
    return TRACK_TYPE_HD;
  if (track_type_string == "UHD1")
    return TRACK_TYPE_UHD1;
  if (track_type_string == "UHD2")
    return TRACK_TYPE_UHD2;
  if (track_type_string == "AUDIO")
    return TRACK_TYPE_AUDIO;
  if (track_type_string == "UNSPECIFIED")
    return TRACK_TYPE_UNSPECIFIED;
  LOG(WARNING) << "Unexpected track type: " << track_type_string;
  return TRACK_TYPE_UNKNOWN;
}

std::string KeySource::TrackTypeToString(TrackType track_type) {
  switch (track_type) {
    case TRACK_TYPE_SD:
      return "SD";
    case TRACK_TYPE_HD:
      return "HD";
    case TRACK_TYPE_UHD1:
      return "UHD1";
    case TRACK_TYPE_UHD2:
      return "UHD2";
    case TRACK_TYPE_AUDIO:
      return "AUDIO";
    default:
      NOTIMPLEMENTED() << "Unknown track type: " << track_type;
      return "UNKNOWN";
  }
}

KeySource::KeySource() {}

}  // namespace media
}  // namespace shaka
