// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_video_client.h"

#include "media/base/video_decoder_config.h"
#include "media/webm/webm_constants.h"

namespace media {

WebMVideoClient::WebMVideoClient(const LogCB& log_cb)
    : log_cb_(log_cb) {
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

bool WebMVideoClient::InitializeConfig(
    const std::string& codec_id, const std::vector<uint8>& codec_private,
    bool is_encrypted, VideoDecoderConfig* config) {
  DCHECK(config);

  VideoCodec video_codec = kUnknownVideoCodec;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  if (codec_id == "V_VP8") {
    video_codec = kCodecVP8;
    profile = VP8PROFILE_MAIN;
  } else if (codec_id == "V_VP9") {
    video_codec = kCodecVP9;
    profile = VP9PROFILE_MAIN;
  } else {
    MEDIA_LOG(log_cb_) << "Unsupported video codec_id " << codec_id;
    return false;
  }

  VideoFrame::Format format =
      (alpha_mode_ == 1) ? VideoFrame::YV12A : VideoFrame::YV12;

  if (pixel_width_ <= 0 || pixel_height_ <= 0)
    return false;

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

  gfx::Size coded_size(pixel_width_, pixel_height_);
  gfx::Rect visible_rect(crop_top_, crop_left_,
                         pixel_width_ - (crop_left_ + crop_right_),
                         pixel_height_ - (crop_top_ + crop_bottom_));
  gfx::Size natural_size = coded_size;
  if (display_unit_ == 0) {
    if (display_width_ <= 0)
      display_width_ = pixel_width_;
    if (display_height_ <= 0)
      display_height_ = pixel_height_;
    natural_size = gfx::Size(display_width_, display_height_);
  } else if (display_unit_ == 3) {
    if (display_width_ <= 0 || display_height_ <= 0)
      return false;
    natural_size = gfx::Size(display_width_, display_height_);
  } else {
    MEDIA_LOG(log_cb_) << "Unsupported display unit type " << display_unit_;
    return false;
  }
  const uint8* extra_data = NULL;
  size_t extra_data_size = 0;
  if (codec_private.size() > 0) {
    extra_data = &codec_private[0];
    extra_data_size = codec_private.size();
  }

  config->Initialize(
      video_codec, profile, format, coded_size, visible_rect, natural_size,
      extra_data, extra_data_size, is_encrypted, true);
  return config->IsValidConfig();
}

bool WebMVideoClient::OnUInt(int id, int64 val) {
  int64* dst = NULL;

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
    MEDIA_LOG(log_cb_) << "Multiple values for id " << std::hex << id
                       << " specified (" << *dst << " and " << val << ")";
    return false;
  }

  *dst = val;
  return true;
}

bool WebMVideoClient::OnBinary(int id, const uint8* data, int size) {
  // Accept binary fields we don't care about for now.
  return true;
}

bool WebMVideoClient::OnFloat(int id, double val) {
  // Accept float fields we don't care about for now.
  return true;
}

}  // namespace media
