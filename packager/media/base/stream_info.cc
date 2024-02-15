// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/stream_info.h>

#include <cinttypes>

#include <absl/log/log.h>
#include <absl/strings/str_format.h>

#include <packager/macros/logging.h>
#include <packager/media/base/timestamp.h>

namespace shaka {
namespace media {

std::string StreamTypeToString(StreamType type) {
  switch (type) {
    case kStreamUnknown:
      return "Unknown";
    case kStreamVideo:
      return "Video";
    case kStreamAudio:
      return "Audio";
    case kStreamText:
      return "Text";
  }

  NOTIMPLEMENTED() << "Unhandled StreamType with value "
                   << static_cast<int>(type);
  return "";
}

StreamInfo::StreamInfo(StreamType stream_type,
                       int track_id,
                       int32_t time_scale,
                       int64_t duration,
                       Codec codec,
                       const std::string& codec_string,
                       const uint8_t* codec_config,
                       size_t codec_config_size,
                       const std::string& language,
                       bool is_encrypted)
    : stream_type_(stream_type),
      track_id_(track_id),
      time_scale_(time_scale),
      duration_(duration),
      codec_(codec),
      codec_string_(codec_string),
      language_(language),
      is_encrypted_(is_encrypted) {
  if (codec_config_size > 0) {
    codec_config_.assign(codec_config, codec_config + codec_config_size);
  }
}

StreamInfo::~StreamInfo() {}

std::string StreamInfo::ToString() const {
  std::string duration;
  if (duration_ == kInfiniteDuration) {
    duration = "Infinite";
  } else {
    duration = absl::StrFormat("%" PRIu64 " (%.1f seconds)", duration_,
                               static_cast<double>(duration_) / time_scale_);
  }

  return absl::StrFormat(
      "type: %s\n codec_string: %s\n time_scale: %d\n duration: "
      "%s\n is_encrypted: %s\n",
      StreamTypeToString(stream_type_).c_str(), codec_string_.c_str(),
      time_scale_, duration.c_str(), is_encrypted_ ? "true" : "false");
}

}  // namespace media
}  // namespace shaka
