// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/video_stream_info.h>

#include <absl/log/log.h>
#include <absl/strings/str_format.h>

#include <packager/macros/logging.h>
#include <packager/media/base/limits.h>

namespace shaka {
namespace media {

namespace {
std::string VideoCodecToString(Codec codec) {
  switch (codec) {
    case kCodecAV1:
      return "AV1";
    case kCodecH264:
      return "H264";
    case kCodecH265:
      return "H265";
    case kCodecH265DolbyVision:
      return "H265 Dolby Vision";
    case kCodecVP8:
      return "VP8";
    case kCodecVP9:
      return "VP9";
    default:
      NOTIMPLEMENTED() << "Unknown Video Codec: " << codec;
      return "UnknownCodec";
  }
}

}  // namespace

VideoStreamInfo::VideoStreamInfo(int track_id,
                                 int32_t time_scale,
                                 int64_t duration,
                                 Codec codec,
                                 H26xStreamFormat h26x_stream_format,
                                 const std::string& codec_string,
                                 const uint8_t* codec_config,
                                 size_t codec_config_size,
                                 uint32_t width,
                                 uint32_t height,
                                 uint32_t pixel_width,
                                 uint32_t pixel_height,
                                 uint8_t transfer_characteristics,
                                 uint32_t trick_play_factor,
                                 uint8_t nalu_length_size,
                                 const std::string& language,
                                 bool is_encrypted)
    : StreamInfo(kStreamVideo,
                 track_id,
                 time_scale,
                 duration,
                 codec,
                 codec_string,
                 codec_config,
                 codec_config_size,
                 language,
                 is_encrypted),
      h26x_stream_format_(h26x_stream_format),
      width_(width),
      height_(height),
      pixel_width_(pixel_width),
      pixel_height_(pixel_height),
      transfer_characteristics_(transfer_characteristics),
      trick_play_factor_(trick_play_factor),
      nalu_length_size_(nalu_length_size) {}

VideoStreamInfo::~VideoStreamInfo() {}

bool VideoStreamInfo::IsValidConfig() const {
  return codec() != kUnknownCodec && width_ > 0 &&
         width_ <= limits::kMaxDimension && height_ > 0 &&
         height_ <= limits::kMaxDimension &&
         (nalu_length_size_ <= 2 || nalu_length_size_ == 4);
}

std::string VideoStreamInfo::ToString() const {
  return absl::StrFormat(
      "%s codec: %s\n width: %d\n height: %d\n pixel_aspect_ratio: %d:%d\n "
      "trick_play_factor: %d\n nalu_length_size: %d\n",
      StreamInfo::ToString().c_str(), VideoCodecToString(codec()).c_str(),
      width_, height_, pixel_width_, pixel_height_, trick_play_factor_,
      nalu_length_size_);
}

std::unique_ptr<StreamInfo> VideoStreamInfo::Clone() const {
  return std::unique_ptr<StreamInfo>(new VideoStreamInfo(*this));
}

}  // namespace media
}  // namespace shaka
