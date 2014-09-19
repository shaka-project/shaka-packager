// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/stream_info.h"

#include <inttypes.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace edash_packager {
namespace media {

StreamInfo::StreamInfo(StreamType stream_type,
                       int track_id,
                       uint32 time_scale,
                       uint64 duration,
                       const std::string& codec_string,
                       const std::string& language,
                       const uint8* extra_data,
                       size_t extra_data_size,
                       bool is_encrypted)
    : stream_type_(stream_type),
      track_id_(track_id),
      time_scale_(time_scale),
      duration_(duration),
      codec_string_(codec_string),
      language_(language),
      is_encrypted_(is_encrypted) {

  if (extra_data_size > 0) {
    extra_data_.assign(extra_data, extra_data + extra_data_size);
  }
}

StreamInfo::~StreamInfo() {}

std::string StreamInfo::ToString() const {
  return base::StringPrintf(
      "type: %s\n codec_string: %s\n time_scale: %d\n duration: %" PRIu64 " "
      "(%.1f seconds)\n language: %s\n is_encrypted: %s\n",
      (stream_type_ == kStreamAudio ? "Audio" : "Video"),
      codec_string_.c_str(),
      time_scale_,
      duration_,
      static_cast<double>(duration_) / time_scale_,
      language_.c_str(),
      is_encrypted_ ? "true" : "false");
}

}  // namespace media
}  // namespace edash_packager
