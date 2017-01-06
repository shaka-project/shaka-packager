// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/vp_codec_configuration_record.h"

#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/rcheck.h"
#include "packager/base/strings/stringprintf.h"

namespace shaka {
namespace media {
namespace {
enum VP9CodecFeatures {
  kFeatureProfile = 1,
  kFeatureLevel = 2,
  kFeatureBitDepth = 3,
  kFeatureChromaSubsampling = 4,
};

std::string VPCodecAsString(Codec codec) {
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

template <typename T>
void MergeField(const std::string& name,
                T source_value,
                bool source_is_set,
                T* dest_value,
                bool* dest_is_set) {
  if (!*dest_is_set || source_is_set) {
    if (*dest_is_set && source_value != *dest_value) {
      LOG(WARNING) << "VPx " << name << " is inconsistent, "
                   << static_cast<uint32_t>(*dest_value) << " vs "
                   << static_cast<uint32_t>(source_value);
    }
    *dest_value = source_value;
    *dest_is_set = true;
  }
}

}  // namespace

VPCodecConfigurationRecord::VPCodecConfigurationRecord() {}

VPCodecConfigurationRecord::VPCodecConfigurationRecord(
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
      profile_is_set_(true),
      level_is_set_(true),
      bit_depth_is_set_(true),
      color_space_is_set_(true),
      chroma_subsampling_is_set_(true),
      transfer_function_is_set_(true),
      video_full_range_flag_is_set_(true),
      codec_initialization_data_(codec_initialization_data) {}

VPCodecConfigurationRecord::~VPCodecConfigurationRecord(){};

bool VPCodecConfigurationRecord::ParseMP4(const std::vector<uint8_t>& data) {
  BitReader reader(data.data(), data.size());
  profile_is_set_ = true;
  level_is_set_ = true;
  bit_depth_is_set_ = true;
  color_space_is_set_ = true;
  chroma_subsampling_is_set_ = true;
  transfer_function_is_set_ = true;
  video_full_range_flag_is_set_ = true;
  RCHECK(reader.ReadBits(8, &profile_));
  RCHECK(reader.ReadBits(8, &level_));
  RCHECK(reader.ReadBits(4, &bit_depth_));
  RCHECK(reader.ReadBits(4, &color_space_));
  RCHECK(reader.ReadBits(4, &chroma_subsampling_));
  RCHECK(reader.ReadBits(3, &transfer_function_));
  RCHECK(reader.ReadBits(1, &video_full_range_flag_));
  uint16_t codec_initialization_data_size = 0;
  RCHECK(reader.ReadBits(16, &codec_initialization_data_size));
  RCHECK(reader.bits_available() >= codec_initialization_data_size * 8u);
  const size_t header_size = data.size() - reader.bits_available() / 8;
  codec_initialization_data_.assign(
      data.begin() + header_size,
      data.begin() + header_size + codec_initialization_data_size);
  return true;
}

bool VPCodecConfigurationRecord::ParseWebM(const std::vector<uint8_t>& data) {
  BufferReader reader(data.data(), data.size());

  while (reader.HasBytes(1)) {
    uint8_t id;
    uint8_t size;
    RCHECK(reader.Read1(&id));
    RCHECK(reader.Read1(&size));

    switch (id) {
      case kFeatureProfile:
        RCHECK(size == 1);
        RCHECK(reader.Read1(&profile_));
        profile_is_set_ = true;
        break;
      case kFeatureLevel:
        RCHECK(size == 1);
        RCHECK(reader.Read1(&level_));
        level_is_set_ = true;
        break;
      case kFeatureBitDepth:
        RCHECK(size == 1);
        RCHECK(reader.Read1(&bit_depth_));
        bit_depth_is_set_ = true;
        break;
      case kFeatureChromaSubsampling:
        RCHECK(size == 1);
        RCHECK(reader.Read1(&chroma_subsampling_));
        chroma_subsampling_is_set_ = true;
        break;
      default: {
        LOG(WARNING) << "Skipping unknown VP9 codec feature " << id;
        RCHECK(reader.SkipBytes(size));
      }
    }
  }

  return true;
}

void VPCodecConfigurationRecord::WriteMP4(std::vector<uint8_t>* data) const {
  BufferWriter writer;
  writer.AppendInt(profile_);
  writer.AppendInt(level_);
  uint8_t bit_depth_color_space = (bit_depth_ << 4) | color_space_;
  writer.AppendInt(bit_depth_color_space);
  uint8_t chroma = (chroma_subsampling_ << 4) | (transfer_function_ << 1) |
                   (video_full_range_flag_ ? 1 : 0);
  writer.AppendInt(chroma);
  uint16_t codec_initialization_data_size =
    static_cast<uint16_t>(codec_initialization_data_.size());
  writer.AppendInt(codec_initialization_data_size);
  writer.AppendVector(codec_initialization_data_);
  writer.SwapBuffer(data);
}

void VPCodecConfigurationRecord::WriteWebM(std::vector<uint8_t>* data) const {
  BufferWriter writer;

  if (profile_is_set_) {
    writer.AppendInt(static_cast<uint8_t>(kFeatureProfile));  // ID = 1
    writer.AppendInt(static_cast<uint8_t>(1));                // Length = 1
    writer.AppendInt(static_cast<uint8_t>(profile_));
  }

  if (level_is_set_ && level_ != 0) {
    writer.AppendInt(static_cast<uint8_t>(kFeatureLevel));  // ID = 2
    writer.AppendInt(static_cast<uint8_t>(1));  // Length = 1
    writer.AppendInt(static_cast<uint8_t>(level_));
  }

  if (bit_depth_is_set_) {
    writer.AppendInt(static_cast<uint8_t>(kFeatureBitDepth));  // ID = 3
    writer.AppendInt(static_cast<uint8_t>(1));  // Length = 1
    writer.AppendInt(static_cast<uint8_t>(bit_depth_));
  }

  if (chroma_subsampling_is_set_) {
    // WebM doesn't differentiate whether it is vertical or collocated with luma
    // for 4:2:0.
    const uint8_t subsampling =
        chroma_subsampling_ == CHROMA_420_COLLOCATED_WITH_LUMA
            ? CHROMA_420_VERTICAL
            : chroma_subsampling_;
    // ID = 4, Length = 1
    writer.AppendInt(static_cast<uint8_t>(kFeatureChromaSubsampling));
    writer.AppendInt(static_cast<uint8_t>(1));
    writer.AppendInt(subsampling);
  }

  writer.SwapBuffer(data);
}

std::string VPCodecConfigurationRecord::GetCodecString(Codec codec) const {
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

void VPCodecConfigurationRecord::MergeFrom(
    const VPCodecConfigurationRecord& other) {
  MergeField("profile", other.profile_, other.profile_is_set_, &profile_,
             &profile_is_set_);
  MergeField("level", other.level_, other.level_is_set_, &level_,
             &level_is_set_);
  MergeField("bit depth", other.bit_depth_, other.bit_depth_is_set_,
             &bit_depth_, &bit_depth_is_set_);
  MergeField("color space", other.color_space_, other.color_space_is_set_,
             &color_space_, &color_space_is_set_);
  MergeField("chroma subsampling", other.chroma_subsampling_,
             other.chroma_subsampling_is_set_, &chroma_subsampling_,
             &chroma_subsampling_is_set_);
  MergeField("transfer function", other.transfer_function_,
             other.transfer_function_is_set_, &transfer_function_,
             &transfer_function_is_set_);
  MergeField("video full range flag", other.video_full_range_flag_,
             other.video_full_range_flag_is_set_, &video_full_range_flag_,
             &video_full_range_flag_is_set_);

  if (codec_initialization_data_.empty() ||
      !other.codec_initialization_data_.empty()) {
    if (!codec_initialization_data_.empty() &&
        codec_initialization_data_ != other.codec_initialization_data_) {
      LOG(WARNING) << "VPx codec initialization data is inconsistent";
    }
    codec_initialization_data_ = other.codec_initialization_data_;
  }
}

}  // namespace media
}  // namespace shaka
