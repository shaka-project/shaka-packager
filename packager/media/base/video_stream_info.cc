// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/video_stream_info.h"

#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/limits.h"

namespace edash_packager {
namespace media {

namespace {
std::string VideoCodecToString(VideoCodec video_codec) {
  switch (video_codec) {
    case kCodecH264:
      return "H264";
    case kCodecVC1:
      return "VC1";
    case kCodecMPEG2:
      return "MPEG2";
    case kCodecMPEG4:
      return "MPEG4";
    case kCodecTheora:
      return "Theora";
    case kCodecVP8:
      return "VP8";
    case kCodecVP9:
      return "VP9";
    default:
      NOTIMPLEMENTED() << "Unknown Video Codec: " << video_codec;
      return "UnknownVideoCodec";
  }
}
}  // namespace

VideoStreamInfo::VideoStreamInfo(int track_id,
                                 uint32_t time_scale,
                                 uint64_t duration,
                                 VideoCodec codec,
                                 const std::string& codec_string,
                                 const std::string& language,
                                 uint16_t width,
                                 uint16_t height,
                                 int16_t trick_play_rate,
                                 uint8_t nalu_length_size,
                                 const uint8_t* extra_data,
                                 size_t extra_data_size,
                                 bool is_encrypted)
    : StreamInfo(kStreamVideo,
                 track_id,
                 time_scale,
                 duration,
                 codec_string,
                 language,
                 extra_data,
                 extra_data_size,
                 is_encrypted),
      codec_(codec),
      width_(width),
      height_(height),
      trick_play_rate_(trick_play_rate),
      nalu_length_size_(nalu_length_size) {
}

VideoStreamInfo::~VideoStreamInfo() {}

bool VideoStreamInfo::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec &&
         width_ > 0 && width_ <= limits::kMaxDimension &&
         height_ > 0 && height_ <= limits::kMaxDimension &&
         (nalu_length_size_ <= 2 || nalu_length_size_ == 4);
}

std::string VideoStreamInfo::ToString() const {
  return base::StringPrintf(
      "%s codec: %s\n width: %d\n height: %d\n trick_play_rate: %d\n"
      " nalu_length_size: %d\n",
      StreamInfo::ToString().c_str(),
      VideoCodecToString(codec_).c_str(),
      width_,
      height_,
      trick_play_rate_,
      nalu_length_size_);
}

std::string VideoStreamInfo::GetCodecString(VideoCodec codec,
                                            uint8_t profile,
                                            uint8_t compatible_profiles,
                                            uint8_t level) {
  switch (codec) {
    case kCodecVP8:
      return "vp8";
    case kCodecVP9:
      return "vp9";
    case kCodecH264: {
      const uint8_t bytes[] = {profile, compatible_profiles, level};
      return "avc1." +
             StringToLowerASCII(base::HexEncode(bytes, arraysize(bytes)));
    }
    default:
      NOTIMPLEMENTED() << "Unknown Codec: " << codec;
      return "unknown";
  }
}

}  // namespace media
}  // namespace edash_packager
