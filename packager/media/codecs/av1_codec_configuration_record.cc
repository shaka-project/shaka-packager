// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/av1_codec_configuration_record.h"

#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/rcheck.h"

namespace shaka {
namespace media {

AV1CodecConfigurationRecord::AV1CodecConfigurationRecord() = default;

AV1CodecConfigurationRecord::~AV1CodecConfigurationRecord() = default;

// https://aomediacodec.github.io/av1-isobmff/#av1codecconfigurationbox-section
// aligned (8) class AV1CodecConfigurationRecord {
//   unsigned int (1) marker = 1;
//   unsigned int (7) version = 1;
//   unsigned int (3) seq_profile;
//   unsigned int (5) seq_level_idx_0;
//   unsigned int (1) seq_tier_0;
//   unsigned int (1) high_bitdepth;
//   unsigned int (1) twelve_bit;
//   unsigned int (1) monochrome;
//   unsigned int (1) chroma_subsampling_x;
//   unsigned int (1) chroma_subsampling_y;
//   unsigned int (2) chroma_sample_position;
//   unsigned int (3) reserved = 0;
//
//   unsigned int (1) initial_presentation_delay_present;
//   if (initial_presentation_delay_present) {
//     unsigned int (4) initial_presentation_delay_minus_one;
//   } else {
//     unsigned int (4) reserved = 0;
//   }
//
//   unsigned int (8)[] configOBUs;
// }
bool AV1CodecConfigurationRecord::Parse(const uint8_t* data, size_t data_size) {
  RCHECK(data_size > 0);

  BitReader reader(data, data_size);

  int marker;
  RCHECK(reader.ReadBits(1, &marker));
  RCHECK(marker == 1);

  int version;
  RCHECK(reader.ReadBits(7, &version));
  RCHECK(version == 1);

  RCHECK(reader.ReadBits(3, &profile_));
  RCHECK(reader.ReadBits(5, &level_));
  RCHECK(reader.ReadBits(1, &tier_));

  int high_bitdepth;
  int twelve_bit;
  RCHECK(reader.ReadBits(1, &high_bitdepth));
  RCHECK(reader.ReadBits(1, &twelve_bit));
  bit_depth_ = twelve_bit ? 12 : (high_bitdepth ? 10 : 8);

  RCHECK(reader.ReadBits(1, &mono_chrome_));
  RCHECK(reader.ReadBits(1, &chroma_subsampling_x_));
  RCHECK(reader.ReadBits(1, &chroma_subsampling_y_));
  RCHECK(reader.ReadBits(2, &chroma_sample_position_));

  // Skip other fields (e.g. initial_presentation_delay) which we do not need.
  return true;
}

// https://aomediacodec.github.io/av1-isobmff/#codecsparam
//   <sample entry 4CC>.<profile>.<level><tier>.<bitDepth>.<monochrome>.
//   <chromaSubsampling>.<colorPrimaries>.<transferCharacteristics>.
//   <matrixCoefficients>.<videoFullRangeFlag>
// The parameters sample entry 4CC, profile, level, tier, and bitDepth are all
// mandatory fields.
// All the other fields (including their leading '.') are optional, mutually
// inclusive (all or none) fields.

// When color info is NOT available, generate the basic codec string without the
// optional fields
std::string AV1CodecConfigurationRecord::GetCodecString() const {
  return base::StringPrintf("av01.%d.%02d%c.%02d", profile_, level_,
                            tier_ ? 'H' : 'M', bit_depth_);
}

// When color info IS available, generate the full codec string with optional
// fields
std::string AV1CodecConfigurationRecord::GetCodecString(
    uint16_t color_primaries,
    uint16_t transfer_characteristics,
    uint16_t matrix_coefficients,
    uint8_t video_full_range_flag) const {
  return base::StringPrintf(
      "av01.%d.%02d%c.%02d.%d.%d%d%d.%02d.%02d.%02d.%d", profile_, level_,
      tier_ ? 'H' : 'M', bit_depth_, mono_chrome_, chroma_subsampling_x_,
      chroma_subsampling_y_, chroma_sample_position_, color_primaries,
      transfer_characteristics, matrix_coefficients, video_full_range_flag);
}

}  // namespace media
}  // namespace shaka
