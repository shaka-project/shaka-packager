// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/text_stream_info.h"

namespace shaka {
namespace media {

TextStreamInfo::TextStreamInfo(int track_id, uint32_t time_scale,
                               uint64_t duration,
                               Codec codec,
                               const std::string& codec_string,
                               const std::string& codec_config, uint16_t width,
                               uint16_t height, const std::string& language)
    : StreamInfo(kStreamText, track_id, time_scale, duration, codec,
                 codec_string,
                 reinterpret_cast<const uint8_t*>(codec_config.data()),
                 codec_config.size(), language, false),
      width_(width),
      height_(height) {}

TextStreamInfo::~TextStreamInfo() {}

bool TextStreamInfo::IsValidConfig() const {
  return true;
}

std::unique_ptr<StreamInfo> TextStreamInfo::Clone() const {
  return std::unique_ptr<StreamInfo>(new TextStreamInfo(*this));
}

}  // namespace media
}  // namespace shaka
