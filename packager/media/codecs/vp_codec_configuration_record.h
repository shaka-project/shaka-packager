// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_CODECS_VP_CODEC_CONFIGURATION_RECORD_H_
#define MEDIA_CODECS_VP_CODEC_CONFIGURATION_RECORD_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "packager/base/macros.h"
#include "packager/media/base/video_stream_info.h"

namespace shaka {
namespace media {

/// Class for parsing or writing VP codec configuration record.
class VPCodecConfigurationRecord {
 public:
  enum ColorSpace {
    COLOR_SPACE_UNSPECIFIED = 0,
    COLOR_SPACE_BT_601 = 1,
    COLOR_SPACE_BT_709 = 2,
    COLOR_SPACE_SMPTE_170 = 3,
    COLOR_SPACE_SMPTE_240 = 4,
    COLOR_SPACE_BT_2020_NON_CONSTANT_LUMINANCE = 5,
    COLOR_SPACE_BT_2020_CONSTANT_LUMINANCE = 6,
    COLOR_SPACE_SRGB = 7,
  };

  enum ChromaSubsampling {
    CHROMA_420_VERTICAL = 0,
    CHROMA_420_COLLOCATED_WITH_LUMA = 1,
    CHROMA_422 = 2,
    CHROMA_444 = 3,
    CHROMA_440 = 4,
  };

  VPCodecConfigurationRecord();
  VPCodecConfigurationRecord(
      uint8_t profile,
      uint8_t level,
      uint8_t bit_depth,
      uint8_t color_space,
      uint8_t chroma_subsampling,
      uint8_t transfer_function,
      bool video_full_range_flag,
      const std::vector<uint8_t>& codec_initialization_data);
  ~VPCodecConfigurationRecord();

  /// Parses input (in MP4 format) to extract VP codec configuration record.
  /// @return false if there is parsing errors.
  bool ParseMP4(const std::vector<uint8_t>& data);

  /// Parses input (in WebM format) to extract VP codec configuration record.
  /// @return false if there is parsing errors.
  bool ParseWebM(const std::vector<uint8_t>& data);

  /// @param data should not be null.
  /// Writes VP codec configuration record to buffer using MP4 format.
  void WriteMP4(std::vector<uint8_t>* data) const;

  /// @param data should not be null.
  /// Writes VP codec configuration record to buffer using WebM format.
  void WriteWebM(std::vector<uint8_t>* data) const;

  /// @return The codec string.
  std::string GetCodecString(VideoCodec codec) const;

  // Merges the values from the given configuration.  If there are values in
  // both |*this| and |other|, the values in |other| take precedence.
  void MergeFrom(const VPCodecConfigurationRecord& other);

  void set_profile(uint8_t profile) {
    profile_ = profile;
    profile_is_set_ = true;
  }
  void set_level(uint8_t level) {
    level_ = level;
    level_is_set_ = true;
  }
  void set_bit_depth(uint8_t bit_depth) {
    bit_depth_ = bit_depth;
    bit_depth_is_set_ = true;
  }
  void set_color_space(uint8_t color_space) {
    color_space_ = color_space;
    color_space_is_set_ = true;
  }
  void set_chroma_subsampling(uint8_t chroma_subsampling) {
    chroma_subsampling_ = chroma_subsampling;
    chroma_subsampling_is_set_ = true;
  }
  void set_transfer_function(uint8_t transfer_function) {
    transfer_function_ = transfer_function;
    transfer_function_is_set_ = true;
  }
  void set_video_full_range_flag(bool video_full_range_flag) {
    video_full_range_flag_ = video_full_range_flag;
  }

  uint8_t profile() const { return profile_; }
  uint8_t level() const { return level_; }
  uint8_t bit_depth() const { return bit_depth_; }
  uint8_t color_space() const { return color_space_; }
  uint8_t chroma_subsampling() const { return chroma_subsampling_; }
  uint8_t transfer_function() const { return transfer_function_; }
  bool video_full_range_flag() const { return video_full_range_flag_; }

 private:
  uint8_t profile_ = 0;
  uint8_t level_ = 0;
  uint8_t bit_depth_ = 0;
  uint8_t color_space_ = 0;
  uint8_t chroma_subsampling_ = 0;
  uint8_t transfer_function_ = 0;
  bool video_full_range_flag_ = false;
  bool profile_is_set_ = false;
  bool level_is_set_ = false;
  bool bit_depth_is_set_ = false;
  bool color_space_is_set_ = false;
  bool chroma_subsampling_is_set_ = false;
  bool transfer_function_is_set_ = false;
  bool video_full_range_flag_is_set_ = false;
  std::vector<uint8_t> codec_initialization_data_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the internal data
  // is small, the performance impact is minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_CODECS_VP_CODEC_CONFIGURATION_RECORD_H_
