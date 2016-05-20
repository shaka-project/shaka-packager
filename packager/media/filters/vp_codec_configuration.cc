// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/vp_codec_configuration.h"

#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/rcheck.h"
#include "packager/base/strings/stringprintf.h"

namespace shaka {
namespace media {
namespace {

std::string VPCodecAsString(VideoCodec codec) {
  switch (codec) {
    case kCodecVP8:
      return "vp08";
    case kCodecVP9:
      return "vp09";
    case kCodecVP10:
      return "vp10";
    default:
      LOG(WARNING) << "Unknown VP codec: " << codec;
      return std::string();
  }
}

}  // namespace

VPCodecConfiguration::VPCodecConfiguration()
    : profile_(0),
      level_(0),
      bit_depth_(0),
      color_space_(0),
      chroma_subsampling_(0),
      transfer_function_(0),
      video_full_range_flag_(false) {}

VPCodecConfiguration::VPCodecConfiguration(
    uint8_t profile,
    uint8_t level,
    uint8_t bit_depth,
    uint8_t color_space,
    uint8_t chroma_subsampling,
    uint8_t transfer_function,
    bool video_full_range_flag,
    const std::vector<uint8_t>& codec_initialization_data)
    : profile_(profile),
      level_(level),
      bit_depth_(bit_depth),
      color_space_(color_space),
      chroma_subsampling_(chroma_subsampling),
      transfer_function_(transfer_function),
      video_full_range_flag_(video_full_range_flag),
      codec_initialization_data_(codec_initialization_data) {}

VPCodecConfiguration::~VPCodecConfiguration(){};

bool VPCodecConfiguration::Parse(const std::vector<uint8_t>& data) {
  BitReader reader(data.data(), data.size());
  RCHECK(reader.ReadBits(8, &profile_));
  RCHECK(reader.ReadBits(8, &level_));
  RCHECK(reader.ReadBits(4, &bit_depth_));
  RCHECK(reader.ReadBits(4, &color_space_));
  RCHECK(reader.ReadBits(4, &chroma_subsampling_));
  RCHECK(reader.ReadBits(3, &transfer_function_));
  RCHECK(reader.ReadBits(1, &video_full_range_flag_));
  uint16_t codec_initialization_data_size = 0;
  RCHECK(reader.ReadBits(16, &codec_initialization_data_size));
  RCHECK(reader.bits_available() >= codec_initialization_data_size * 8);
  const size_t header_size = data.size() - reader.bits_available() / 8;
  codec_initialization_data_.assign(
      data.begin() + header_size,
      data.begin() + header_size + codec_initialization_data_size);
  return true;
}

void VPCodecConfiguration::Write(std::vector<uint8_t>* data) const {
  BufferWriter writer;
  writer.AppendInt(profile_);
  writer.AppendInt(level_);
  uint8_t bit_depth_color_space = (bit_depth_ << 4) | color_space_;
  writer.AppendInt(bit_depth_color_space);
  uint8_t chroma = (chroma_subsampling_ << 4) | (transfer_function_ << 1) |
                   (video_full_range_flag_ ? 1 : 0);
  writer.AppendInt(chroma);
  uint16_t codec_initialization_data_size = codec_initialization_data_.size();
  writer.AppendInt(codec_initialization_data_size);
  writer.AppendVector(codec_initialization_data_);
  writer.SwapBuffer(data);
}

std::string VPCodecConfiguration::GetCodecString(VideoCodec codec) const {
  const std::string fields[] = {
      base::IntToString(profile_),
      base::IntToString(level_),
      base::IntToString(bit_depth_),
      base::IntToString(color_space_),
      base::IntToString(chroma_subsampling_),
      base::IntToString(transfer_function_),
      (video_full_range_flag_ ? "01" : "00"),
  };

  std::string codec_string = VPCodecAsString(codec);
  for (const std::string& field : fields) {
    // Make sure every field is at least 2-chars wide. The space will be
    // replaced with '0' afterwards.
    base::StringAppendF(&codec_string, ".%2s", field.c_str());
  }
  base::ReplaceChars(codec_string, " ", "0", &codec_string);
  return codec_string;
}

}  // namespace media
}  // namespace shaka
