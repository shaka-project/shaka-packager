// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/webm/webm_video_client.h>

#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/video_util.h>
#include <packager/media/codecs/av1_codec_configuration_record.h>
#include <packager/media/codecs/vp_codec_configuration_record.h>
#include <packager/media/formats/webm/webm_constants.h>

namespace {

// Timestamps are represented in double in WebM. Convert to int64_t in us.
const int32_t kWebMTimeScale = 1000000;

}  // namespace

namespace shaka {
namespace media {

WebMVideoClient::WebMVideoClient() {}

WebMVideoClient::~WebMVideoClient() {}

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

  matrix_coefficients_ = -1;
  bits_per_channel_ = -1;
  chroma_subsampling_horz_ = -1;
  chroma_subsampling_vert_ = -1;
  chroma_siting_horz_ = -1;
  chroma_siting_vert_ = -1;
  color_range_ = -1;
  transfer_characteristics_ = -1;
  color_primaries_ = -1;
}

std::shared_ptr<VideoStreamInfo> WebMVideoClient::GetVideoStreamInfo(
    int64_t track_num,
    const std::string& codec_id,
    const std::vector<uint8_t>& codec_private,
    bool is_encrypted) {
  std::string codec_string;
  Codec video_codec = kUnknownCodec;
  if (codec_id == "V_AV1") {
    video_codec = kCodecAV1;

    // CodecPrivate is mandatory per AV in Matroska / WebM specification.
    // https://github.com/Matroska-Org/matroska-specification/blob/av1-mappin/codec/av1.md#codecprivate-1
    AV1CodecConfigurationRecord av1_config;
    if (!av1_config.Parse(codec_private)) {
      LOG(ERROR) << "Failed to parse AV1 codec_private.";
      return nullptr;
    }
    codec_string = av1_config.GetCodecString();
  } else if (codec_id == "V_VP8") {
    video_codec = kCodecVP8;
    // codec_string for VP8 is parsed later.
  } else if (codec_id == "V_VP9") {
    video_codec = kCodecVP9;
    // codec_string for VP9 is parsed later.
  } else {
    LOG(ERROR) << "Unsupported video codec_id " << codec_id;
    return nullptr;
  }

  if (pixel_width_ <= 0 || pixel_height_ <= 0)
    return nullptr;

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
      return nullptr;
  } else {
    LOG(ERROR) << "Unsupported display unit type " << display_unit_;
    return nullptr;
  }

  // Calculate sample aspect ratio.
  uint32_t pixel_width;
  uint32_t pixel_height;
  DerivePixelWidthHeight(width_after_crop, height_after_crop, display_width_,
                         display_height_, &pixel_width, &pixel_height);

  // |codec_private| may be overriden later for some codecs, e.g. VP9 since for
  // VP9, the format for MP4 and WebM are different; MP4 format is used as the
  // intermediate format.
  return std::make_shared<VideoStreamInfo>(
      track_num, kWebMTimeScale, 0, video_codec, H26xStreamFormat::kUnSpecified,
      codec_string, codec_private.data(), codec_private.size(),
      width_after_crop, height_after_crop, pixel_width, pixel_height, 0, 0,
      0 /* transfer_characteristics */, std::string(), is_encrypted);
}

VPCodecConfigurationRecord WebMVideoClient::GetVpCodecConfig(
    const std::vector<uint8_t>& codec_private) {
  VPCodecConfigurationRecord vp_config;
  vp_config.ParseWebM(codec_private);
  if (matrix_coefficients_ != -1) {
    vp_config.set_matrix_coefficients(matrix_coefficients_);
  }
  if (bits_per_channel_ != -1) {
    vp_config.set_bit_depth(bits_per_channel_);
  }
  if (chroma_subsampling_horz_ != -1 && chroma_subsampling_vert_ != -1) {
    vp_config.SetChromaSubsampling(chroma_subsampling_horz_,
                                   chroma_subsampling_vert_);
  }
  if (chroma_siting_horz_ != -1 && chroma_siting_vert_ != -1) {
    vp_config.SetChromaLocation(chroma_siting_horz_, chroma_siting_vert_);
  }
  if (color_range_ != -1) {
    if (color_range_ == 0)
      vp_config.set_video_full_range_flag(false);
    else if (color_range_ == 1)
      vp_config.set_video_full_range_flag(true);
    // Ignore for other values.
  }
  if (transfer_characteristics_ != -1) {
    vp_config.set_transfer_characteristics(transfer_characteristics_);
  }
  if (color_primaries_ != -1) {
    vp_config.set_color_primaries(color_primaries_);
  }
  return vp_config;
}

WebMParserClient* WebMVideoClient::OnListStart(int id) {
  return id == kWebMIdColor || id == kWebMIdProjection
             ? this
             : WebMParserClient::OnListStart(id);
}

bool WebMVideoClient::OnListEnd(int id) {
  return id == kWebMIdColor || id == kWebMIdProjection
             ? true
             : WebMParserClient::OnListEnd(id);
}

bool WebMVideoClient::OnUInt(int id, int64_t val) {
  int64_t* dst = nullptr;

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
    case kWebMIdColorMatrixCoefficients:
      dst = &matrix_coefficients_;
      break;
    case kWebMIdColorBitsPerChannel:
      dst = &bits_per_channel_;
      break;
    case kWebMIdColorChromaSubsamplingHorz:
      dst = &chroma_subsampling_horz_;
      break;
    case kWebMIdColorChromaSubsamplingVert:
      dst = &chroma_subsampling_vert_;
      break;
    case kWebMIdColorChromaSitingHorz:
      dst = &chroma_siting_horz_;
      break;
    case kWebMIdColorChromaSitingVert:
      dst = &chroma_siting_vert_;
      break;
    case kWebMIdColorRange:
      dst = &color_range_;
      break;
    case kWebMIdColorTransferCharacteristics:
      dst = &transfer_characteristics_;
      break;
    case kWebMIdColorPrimaries:
      dst = &color_primaries_;
      break;
    case kWebMIdColorMaxCLL:
    case kWebMIdColorMaxFALL:
      NOTIMPLEMENTED() << "HDR is not supported yet.";
      return true;
    case kWebMIdProjectionType:
      LOG(WARNING) << "Ignoring ProjectionType with value " << val;
      return true;
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

bool WebMVideoClient::OnBinary(int /*id*/,
                               const uint8_t* /*data*/,
                               int /*size*/) {
  // Accept binary fields we don't care about for now.
  return true;
}

bool WebMVideoClient::OnFloat(int /*id*/, double /*val*/) {
  // Accept float fields we don't care about for now.
  return true;
}

}  // namespace media
}  // namespace shaka
