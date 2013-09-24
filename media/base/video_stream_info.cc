// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_stream_info.h"

#include <sstream>

#include "media/base/limits.h"

namespace media {

VideoStreamInfo::VideoStreamInfo(int track_id,
                                 int time_scale,
                                 VideoCodec codec,
                                 int width,
                                 int height,
                                 const uint8* extra_data,
                                 size_t extra_data_size,
                                 bool is_encrypted)
    : StreamInfo(kStreamVideo,
                 track_id,
                 time_scale,
                 extra_data,
                 extra_data_size,
                 is_encrypted),
      codec_(codec),
      width_(width),
      height_(height) {}

VideoStreamInfo::~VideoStreamInfo() {}

bool VideoStreamInfo::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec &&
         width_ > 0 && width_ <= limits::kMaxDimension &&
         height_ > 0 && height_ <= limits::kMaxDimension;
}

std::string VideoStreamInfo::ToString() {
  std::ostringstream s;
  s << "codec: " << codec_
    << " width: " << width_
    << " height: " << height_
    << " " << StreamInfo::ToString();
  return s.str();
}

}  // namespace media
