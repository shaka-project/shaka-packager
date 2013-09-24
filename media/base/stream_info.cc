// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/stream_info.h"

#include "base/logging.h"

namespace media {

StreamInfo::StreamInfo(StreamType stream_type,
                       int track_id,
                       int time_scale,
                       const uint8* extra_data,
                       size_t extra_data_size,
                       bool is_encrypted)
    : stream_type_(stream_type),
      track_id_(track_id),
      time_scale_(time_scale),
      is_encrypted_(is_encrypted) {

  CHECK((extra_data_size != 0) == (extra_data != NULL));
  extra_data_.assign(extra_data, extra_data + extra_data_size);
}

StreamInfo::~StreamInfo() {}

std::string StreamInfo::ToString() {
  std::ostringstream s;
  s << "type: " << (stream_type_ == kStreamAudio ? "Audio" : "Video")
    << " track_id: " << track_id_
    << " time_scale: " << time_scale_
    << " is_encrypted: " << is_encrypted_;
  return s.str();
}

}  // namespace media
