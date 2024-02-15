// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/vp_codec_configuration_record.h>

#include <absl/strings/str_format.h>
#include <absl/strings/str_replace.h>

#include <packager/macros/logging.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/base/buffer_reader.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/rcheck.h>

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
    default:
      LOG(WARNING) << "Unknown VP codec: " << codec;
      return std::string();
  }
}

template <typename T>
void MergeField(const std::string& name,
                const std::optional<T>& source_value,
                std::optional<T>* dest_value) {
  if (*dest_value) {
    if (source_value && *source_value != **dest_value) {
      LOG(WARNING) << "VPx " << name << " is inconsistent, "
                   << static_cast<int>(**dest_value) << " vs "
                   << static_cast<int>(*source_value);
    }
  } else {
    // Only set dest_value if it is not set.
    *dest_value = source_value;
  }
}

enum VP9Level {
  LEVEL_UNKNOWN = 0,
  LEVEL_1 = 10,
  LEVEL_1_1 = 11,
  LEVEL_2 = 20,
  LEVEL_2_1 = 21,
  LEVEL_3 = 30,
  LEVEL_3_1 = 31,
  LEVEL_4 = 40,
  LEVEL_4_1 = 41,
  LEVEL_5 = 50,
  LEVEL_5_1 = 51,
  LEVEL_5_2 = 52,
  LEVEL_6 = 60,
  LEVEL_6_1 = 61,
  LEVEL_6_2 = 62,
  LEVEL_MAX = 255
};

struct VP9LevelCharacteristics {
  uint64_t max_luma_sample_rate;
  uint32_t max_luma_picture_size;
  double max_avg_bitrate;
  double max_cpb_size;
  double min_compression_ratio;
  uint8_t max_num_column_tiles;
  uint32_t min_altref_distance;
  uint8_t max_ref_frame_buffers;
};

struct VP9LevelDefinition {
  VP9Level level;
  VP9LevelCharacteristics characteristics;
};

VP9Level LevelFromCharacteristics(uint64_t luma_sample_rate,
                                  uint32_t luma_picture_size) {
  // https://www.webmproject.org/vp9/levels/.
  const VP9LevelDefinition vp9_level_definitions[] = {
      {LEVEL_1, {829440, 36864, 200, 400, 2, 1, 4, 8}},
      {LEVEL_1_1, {2764800, 73728, 800, 1000, 2, 1, 4, 8}},
      {LEVEL_2, {4608000, 122880, 1800, 1500, 2, 1, 4, 8}},
      {LEVEL_2_1, {9216000, 245760, 3600, 2800, 2, 2, 4, 8}},
      {LEVEL_3, {20736000, 552960, 7200, 6000, 2, 4, 4, 8}},
      {LEVEL_3_1, {36864000, 983040, 12000, 10000, 2, 4, 4, 8}},
      {LEVEL_4, {83558400, 2228224, 18000, 16000, 4, 4, 4, 8}},
      {LEVEL_4_1, {160432128, 2228224, 30000, 18000, 4, 4, 5, 6}},
      {LEVEL_5, {311951360, 8912896, 60000, 36000, 6, 8, 6, 4}},
      {LEVEL_5_1, {588251136, 8912896, 120000, 46000, 8, 8, 10, 4}},
      {LEVEL_5_2, {1176502272, 8912896, 180000, 90000, 8, 8, 10, 4}},
      {LEVEL_6, {1176502272, 35651584, 180000, 90000, 8, 16, 10, 4}},
      {LEVEL_6_1, {2353004544u, 35651584, 240000, 180000, 8, 16, 10, 4}},
      {LEVEL_6_2, {4706009088u, 35651584, 480000, 360000, 8, 16, 10, 4}},
  };

  for (const VP9LevelDefinition& def : vp9_level_definitions) {
    // All the characteristic fields except max_luma_sample_rate and
    // max_luma_picture_size are ignored to avoid the extra complexities of
    // computing those values. It may result in incorrect level being returned.
    // If this is a problem, please file a bug to
    // https://github.com/shaka-project/shaka-packager/issues.
    if (luma_sample_rate <= def.characteristics.max_luma_sample_rate &&
        luma_picture_size <= def.characteristics.max_luma_picture_size) {
      return def.level;
    }
  }

  LOG(WARNING) << "Cannot determine VP9 level for luma_sample_rate ("
               << luma_sample_rate << ") or luma_picture_size ("
               << luma_picture_size << "). Returning LEVEL_1.";
  return LEVEL_1;
}

}  // namespace

VPCodecConfigurationRecord::VPCodecConfigurationRecord() {}

VPCodecConfigurationRecord::VPCodecConfigurationRecord(
    uint8_t profile,
    uint8_t level,
    uint8_t bit_depth,
    uint8_t chroma_subsampling,
    bool video_full_range_flag,
    uint8_t color_primaries,
    uint8_t transfer_characteristics,
    uint8_t matrix_coefficients,
    const std::vector<uint8_t>& codec_initialization_data)
    : profile_(profile),
      level_(level),
      bit_depth_(bit_depth),
      chroma_subsampling_(chroma_subsampling),
      video_full_range_flag_(video_full_range_flag),
      color_primaries_(color_primaries),
      transfer_characteristics_(transfer_characteristics),
      matrix_coefficients_(matrix_coefficients),
      codec_initialization_data_(codec_initialization_data) {}

VPCodecConfigurationRecord::~VPCodecConfigurationRecord(){};

// https://www.webmproject.org/vp9/mp4/
bool VPCodecConfigurationRecord::ParseMP4(const std::vector<uint8_t>& data) {
  BitReader reader(data.data(), data.size());
  uint8_t value;
  RCHECK(reader.ReadBits(8, &value));
  profile_ = value;
  RCHECK(reader.ReadBits(8, &value));
  level_ = value;
  RCHECK(reader.ReadBits(4, &value));
  bit_depth_ = value;
  RCHECK(reader.ReadBits(3, &value));
  chroma_subsampling_ = value;
  bool bool_value;
  RCHECK(reader.ReadBits(1, &bool_value));
  video_full_range_flag_ = bool_value;
  RCHECK(reader.ReadBits(8, &value));
  color_primaries_ = value;
  RCHECK(reader.ReadBits(8, &value));
  transfer_characteristics_ = value;
  RCHECK(reader.ReadBits(8, &value));
  matrix_coefficients_ = value;

  uint16_t codec_initialization_data_size = 0;
  RCHECK(reader.ReadBits(16, &codec_initialization_data_size));
  RCHECK(reader.bits_available() >= codec_initialization_data_size * 8u);
  const size_t header_size = data.size() - reader.bits_available() / 8;
  codec_initialization_data_.assign(
      data.begin() + header_size,
      data.begin() + header_size + codec_initialization_data_size);
  return true;
}

// http://wiki.webmproject.org/vp9-codecprivate
bool VPCodecConfigurationRecord::ParseWebM(const std::vector<uint8_t>& data) {
  BufferReader reader(data.data(), data.size());

  while (reader.HasBytes(1)) {
    uint8_t id;
    uint8_t size;
    RCHECK(reader.Read1(&id));
    RCHECK(reader.Read1(&size));

    uint8_t value = 0;
    switch (id) {
      case kFeatureProfile:
        RCHECK(size == 1);
        RCHECK(reader.Read1(&value));
        profile_ = value;
        break;
      case kFeatureLevel:
        RCHECK(size == 1);
        RCHECK(reader.Read1(&value));
        level_ = value;
        break;
      case kFeatureBitDepth:
        RCHECK(size == 1);
        RCHECK(reader.Read1(&value));
        bit_depth_ = value;
        break;
      case kFeatureChromaSubsampling:
        RCHECK(size == 1);
        RCHECK(reader.Read1(&value));
        chroma_subsampling_ = value;
        break;
      default: {
        LOG(WARNING) << "Skipping unknown VP9 codec feature " << id;
        RCHECK(reader.SkipBytes(size));
      }
    }
  }

  return true;
}

void VPCodecConfigurationRecord::SetVP9Level(uint16_t width,
                                             uint16_t height,
                                             double sample_duration_seconds) {
  // https://www.webmproject.org/vp9/levels/.

  const uint32_t luma_picture_size = width * height;
  // Alt-Ref frames are not taken into consideration intentionally to avoid the
  // extra complexities. It may result in smaller luma_sample_rate may than the
  // actual luma_sample_rate, leading to incorrect level being returned.
  // If this is a problem, please file a bug to
  // https://github.com/shaka-project/shaka-packager/issues.
  const double kUnknownSampleDuration = 0.0;
  // The decision is based on luma_picture_size only if duration is unknown.
  uint64_t luma_sample_rate = 0;
  if (sample_duration_seconds != kUnknownSampleDuration)
    luma_sample_rate = luma_picture_size / sample_duration_seconds;

  level_ = LevelFromCharacteristics(luma_sample_rate, luma_picture_size);
}

void VPCodecConfigurationRecord::WriteMP4(std::vector<uint8_t>* data) const {
  BufferWriter writer;
  writer.AppendInt(profile());
  writer.AppendInt(level());
  uint8_t bit_depth_chroma = (bit_depth() << 4) | (chroma_subsampling() << 1) |
                             (video_full_range_flag() ? 1 : 0);
  writer.AppendInt(bit_depth_chroma);
  writer.AppendInt(color_primaries());
  writer.AppendInt(transfer_characteristics());
  writer.AppendInt(matrix_coefficients());
  uint16_t codec_initialization_data_size =
    static_cast<uint16_t>(codec_initialization_data_.size());
  writer.AppendInt(codec_initialization_data_size);
  writer.AppendVector(codec_initialization_data_);
  writer.SwapBuffer(data);
}

void VPCodecConfigurationRecord::WriteWebM(std::vector<uint8_t>* data) const {
  BufferWriter writer;

  if (profile_) {
    writer.AppendInt(static_cast<uint8_t>(kFeatureProfile));  // ID = 1
    writer.AppendInt(static_cast<uint8_t>(1));                // Length = 1
    writer.AppendInt(*profile_);
  }

  if (level_) {
    writer.AppendInt(static_cast<uint8_t>(kFeatureLevel));  // ID = 2
    writer.AppendInt(static_cast<uint8_t>(1));  // Length = 1
    writer.AppendInt(*level_);
  }

  if (bit_depth_) {
    writer.AppendInt(static_cast<uint8_t>(kFeatureBitDepth));  // ID = 3
    writer.AppendInt(static_cast<uint8_t>(1));  // Length = 1
    writer.AppendInt(*bit_depth_);
  }

  if (chroma_subsampling_) {
    // ID = 4, Length = 1
    writer.AppendInt(static_cast<uint8_t>(kFeatureChromaSubsampling));
    writer.AppendInt(static_cast<uint8_t>(1));
    writer.AppendInt(*chroma_subsampling_);
  }

  writer.SwapBuffer(data);
}

std::string VPCodecConfigurationRecord::GetCodecString(Codec codec) const {
  const std::string fields[] = {
      absl::StrFormat("%d", profile()),
      absl::StrFormat("%d", level()),
      absl::StrFormat("%d", bit_depth()),
      absl::StrFormat("%d", chroma_subsampling()),
      absl::StrFormat("%d", color_primaries()),
      absl::StrFormat("%d", transfer_characteristics()),
      absl::StrFormat("%d", matrix_coefficients()),
      (video_full_range_flag_ && *video_full_range_flag_) ? "01" : "00",
  };

  std::string codec_string = VPCodecAsString(codec);
  for (const std::string& field : fields) {
    // Make sure every field is at least 2-chars wide. The space will be
    // replaced with '0' afterwards.
    absl::StrAppendFormat(&codec_string, ".%2s", field.c_str());
  }
  absl::StrReplaceAll({{" ", "0"}}, &codec_string);
  return codec_string;
}

void VPCodecConfigurationRecord::MergeFrom(
    const VPCodecConfigurationRecord& other) {
  MergeField("profile", other.profile_, &profile_);
  MergeField("level", other.level_, &level_);
  MergeField("bit depth", other.bit_depth_, &bit_depth_);
  MergeField("chroma subsampling", other.chroma_subsampling_,
             &chroma_subsampling_);
  MergeField("video full range flag", other.video_full_range_flag_,
             &video_full_range_flag_);
  MergeField("color primaries", other.color_primaries_, &color_primaries_);
  MergeField("transfer characteristics", other.transfer_characteristics_,
             &transfer_characteristics_);
  MergeField("matrix coefficients", other.matrix_coefficients_,
             &matrix_coefficients_);

  if (codec_initialization_data_.empty() ||
      !other.codec_initialization_data_.empty()) {
    if (!codec_initialization_data_.empty() &&
        codec_initialization_data_ != other.codec_initialization_data_) {
      LOG(WARNING) << "VPx codec initialization data is inconsistent";
    }
    codec_initialization_data_ = other.codec_initialization_data_;
  }

  MergeField("chroma location", other.chroma_location_, &chroma_location_);
  UpdateChromaSubsamplingIfNeeded();
}

void VPCodecConfigurationRecord::SetChromaSubsampling(uint8_t subsampling_x,
                                                      uint8_t subsampling_y) {
  VLOG(3) << "Set Chroma subsampling " << static_cast<int>(subsampling_x) << " "
          << static_cast<int>(subsampling_y);
  if (subsampling_x == 0 && subsampling_y == 0) {
    chroma_subsampling_ = CHROMA_444;
  } else if (subsampling_x == 0 && subsampling_y == 1) {
    chroma_subsampling_ = CHROMA_440;
  } else if (subsampling_x == 1 && subsampling_y == 0) {
    chroma_subsampling_ = CHROMA_422;
  } else if (subsampling_x == 1 && subsampling_y == 1) {
    // VP9 assumes that chrome samples are collocated with luma samples if
    // there is no explicit signaling outside of VP9 bitstream.
    chroma_subsampling_ = CHROMA_420_COLLOCATED_WITH_LUMA;
  } else {
    LOG(WARNING) << "Unexpected chroma subsampling values: "
                 << static_cast<int>(subsampling_x) << " "
                 << static_cast<int>(subsampling_y);
  }
  UpdateChromaSubsamplingIfNeeded();
}

void VPCodecConfigurationRecord::SetChromaSubsampling(
    ChromaSubsampling chroma_subsampling) {
  chroma_subsampling_ = chroma_subsampling;
  UpdateChromaSubsamplingIfNeeded();
}

void VPCodecConfigurationRecord::SetChromaLocation(uint8_t chroma_siting_x,
                                                   uint8_t chroma_siting_y) {
  VLOG(3) << "Set Chroma Location " << static_cast<int>(chroma_siting_x) << " "
          << static_cast<int>(chroma_siting_y);
  if (chroma_siting_x == kLeftCollocated && chroma_siting_y == kTopCollocated) {
    chroma_location_ = AVCHROMA_LOC_TOPLEFT;
  } else if (chroma_siting_x == kLeftCollocated && chroma_siting_y == kHalf) {
    chroma_location_ = AVCHROMA_LOC_LEFT;
  } else if (chroma_siting_x == kHalf && chroma_siting_y == kTopCollocated) {
    chroma_location_ = AVCHROMA_LOC_TOP;
  } else if (chroma_siting_x == kHalf && chroma_siting_y == kHalf) {
    chroma_location_ = AVCHROMA_LOC_CENTER;
  } else {
    LOG(WARNING) << "Unexpected chroma siting values: "
                 << static_cast<int>(chroma_siting_x) << " "
                 << static_cast<int>(chroma_siting_y);
  }
  UpdateChromaSubsamplingIfNeeded();
}

void VPCodecConfigurationRecord::UpdateChromaSubsamplingIfNeeded() {
  // Use chroma location to fix the chroma subsampling format.
  if (chroma_location_ && chroma_subsampling_ &&
      (*chroma_subsampling_ == CHROMA_420_VERTICAL ||
       *chroma_subsampling_ == CHROMA_420_COLLOCATED_WITH_LUMA)) {
    if (*chroma_location_ == AVCHROMA_LOC_TOPLEFT)
      chroma_subsampling_ = CHROMA_420_COLLOCATED_WITH_LUMA;
    else if (*chroma_location_ == AVCHROMA_LOC_LEFT)
      chroma_subsampling_ = CHROMA_420_VERTICAL;
    VLOG(3) << "Chroma subsampling " << static_cast<int>(*chroma_subsampling_);
  }
}

}  // namespace media
}  // namespace shaka
