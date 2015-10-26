// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/webm/webm_video_client.h"

#include "packager/base/logging.h"
#include "packager/base/stl_util.h"
#include "packager/media/filters/vp_codec_configuration.h"
#include "packager/media/formats/webm/webm_constants.h"

namespace {

// Timestamps are represented in double in WebM. Convert to uint64_t in us.
const uint32_t kWebMTimeScale = 1000000u;

int64_t GetGreatestCommonDivisor(int64_t a, int64_t b) {
  while (b) {
    int64_t temp = b;
    b = a % b;
    a = temp;
  }
  return a;
}

}  // namespace

namespace edash_packager {
namespace media {

WebMVideoClient::WebMVideoClient() {
  Reset();
}

WebMVideoClient::~WebMVideoClient() {
}

void WebMVideoClient::Reset() {
  pixel_width_ = -1;
  pixel_height_ = -1;
  crop_bottom_ = -1;
  crop_top_ = -1;
  crop_left_ = -1;
  crop_right_ = -1;
  display_width_ = -1;
  display_height_ = -1;
  display_unit_ = -1;
  alpha_mode_ = -1;
}

scoped_refptr<VideoStreamInfo> WebMVideoClient::GetVideoStreamInfo(
    int64_t track_num,
    const std::string& codec_id,
    const std::vector<uint8_t>& codec_private,
    bool is_encrypted) {
  VideoCodec video_codec = kUnknownVideoCodec;
  if (codec_id == "V_VP8") {
    video_codec = kCodecVP8;
  } else if (codec_id == "V_VP9") {
    video_codec = kCodecVP9;
  } else if (codec_id == "V_VP10") {
    video_codec = kCodecVP10;
  } else {
    LOG(ERROR) << "Unsupported video codec_id " << codec_id;
    return scoped_refptr<VideoStreamInfo>();
  }

  if (pixel_width_ <= 0 || pixel_height_ <= 0)
    return scoped_refptr<VideoStreamInfo>();

  // Set crop and display unit defaults if these elements are not present.
  if (crop_bottom_ == -1)
    crop_bottom_ = 0;

  if (crop_top_ == -1)
    crop_top_ = 0;

  if (crop_left_ == -1)
    crop_left_ = 0;

  if (crop_right_ == -1)
    crop_right_ = 0;

  if (display_unit_ == -1)
    display_unit_ = 0;

  uint16_t width_after_crop = pixel_width_ - (crop_left_ + crop_right_);
  uint16_t height_after_crop = pixel_height_ - (crop_top_ + crop_bottom_);

  if (display_unit_ == 0) {
    if (display_width_ <= 0)
      display_width_ = width_after_crop;
    if (display_height_ <= 0)
      display_height_ = height_after_crop;
  } else if (display_unit_ == 3) {
    if (display_width_ <= 0 || display_height_ <= 0)
      return scoped_refptr<VideoStreamInfo>();
  } else {
    LOG(ERROR) << "Unsupported display unit type " << display_unit_;
    return scoped_refptr<VideoStreamInfo>();
  }
  // Calculate sample aspect ratio.
  int64_t sar_x = display_width_ * height_after_crop;
  int64_t sar_y = display_height_ * width_after_crop;
  int64_t gcd = GetGreatestCommonDivisor(sar_x, sar_y);
  sar_x /= gcd;
  sar_y /= gcd;

  // TODO(kqyang): Fill in the values for vp codec configuration.
  const uint8_t profile = 0;
  const uint8_t level = 0;
  const uint8_t bit_depth = 8;
  const uint8_t color_space = 0;
  const uint8_t chroma_subsampling = 0;
  const uint8_t transfer_function = 0;
  const bool video_full_range_flag = false;
  VPCodecConfiguration vp_config(profile, level, bit_depth, color_space,
                                 chroma_subsampling, transfer_function,
                                 video_full_range_flag, codec_private);
  std::vector<uint8_t> extra_data;
  vp_config.Write(&extra_data);

  return scoped_refptr<VideoStreamInfo>(new VideoStreamInfo(
      track_num, kWebMTimeScale, 0, video_codec,
      vp_config.GetCodecString(video_codec), std::string(), width_after_crop,
      height_after_crop, sar_x, sar_y, 0, 0, vector_as_array(&extra_data),
      extra_data.size(), is_encrypted));
}

bool WebMVideoClient::OnUInt(int id, int64_t val) {
  int64_t* dst = NULL;

  switch (id) {
    case kWebMIdPixelWidth:
      dst = &pixel_width_;
      break;
    case kWebMIdPixelHeight:
      dst = &pixel_height_;
      break;
    case kWebMIdPixelCropTop:
      dst = &crop_top_;
      break;
    case kWebMIdPixelCropBottom:
      dst = &crop_bottom_;
      break;
    case kWebMIdPixelCropLeft:
      dst = &crop_left_;
      break;
    case kWebMIdPixelCropRight:
      dst = &crop_right_;
      break;
    case kWebMIdDisplayWidth:
      dst = &display_width_;
      break;
    case kWebMIdDisplayHeight:
      dst = &display_height_;
      break;
    case kWebMIdDisplayUnit:
      dst = &display_unit_;
      break;
    case kWebMIdAlphaMode:
      dst = &alpha_mode_;
      break;
    default:
      return true;
  }

  if (*dst != -1) {
    LOG(ERROR) << "Multiple values for id " << std::hex << id << " specified ("
               << *dst << " and " << val << ")";
    return false;
  }

  *dst = val;
  return true;
}

bool WebMVideoClient::OnBinary(int id, const uint8_t* data, int size) {
  // Accept binary fields we don't care about for now.
  return true;
}

bool WebMVideoClient::OnFloat(int id, double val) {
  // Accept float fields we don't care about for now.
  return true;
}

}  // namespace media
}  // namespace edash_packager
