// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILTERS_VP_CODEC_CONFIGURATION_H_
#define MEDIA_FILTERS_VP_CODEC_CONFIGURATION_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "packager/base/macros.h"
#include "packager/media/base/video_stream_info.h"

namespace edash_packager {
namespace media {

/// Class for parsing or writing VP codec configuration data.
class VPCodecConfiguration {
 public:
  enum ColorSpace {
    COLOR_SPACE_UNSPECIFIED = 0,
    COLOR_SPACE_BT_601 = 1,
    COLOR_SPACE_BT_709 = 2,
    COLOR_SPACE_BT_2020_NON_CONSTANT_LUMINANCE = 3,
    COLOR_SPACE_BT_2020_CONSTANT_LUMINANCE = 4,
    COLOR_SPACE_SRGB = 5,
  };

  enum ChromaSubsampling {
    CHROMA_420_VERTICAL = 0,
    CHROMA_420_COLLOCATED_WITH_LUMA = 1,
    CHROMA_422 = 2,
    CHROMA_444 = 3,
    CHROMA_440 = 4,
  };

  VPCodecConfiguration();
  VPCodecConfiguration(uint8_t profile,
                       uint8_t level,
                       uint8_t bit_depth,
                       uint8_t color_space,
                       uint8_t chroma_subsampling,
                       uint8_t transfer_function,
                       bool video_full_range_flag,
                       const std::vector<uint8_t>& codec_initialization_data);
  ~VPCodecConfiguration();

  /// Parses input to extract VP codec configuration data.
  /// @return false if there is parsing errors.
  bool Parse(const std::vector<uint8_t>& data);

  /// @param data should not be null.
  /// Writes VP codec configuration data to buffer.
  void Write(std::vector<uint8_t>* data) const;

  /// @return The codec string.
  std::string GetCodecString(VideoCodec codec) const;

  void set_profile(uint8_t profile) { profile_ = profile; }
  void set_level(uint8_t level) { level_ = level; }
  void set_bit_depth(uint8_t bit_depth) { bit_depth_ = bit_depth; }
  void set_color_space(uint8_t color_space) { color_space_ = color_space; }
  void set_chroma_subsampling(uint8_t chroma_subsampling) {
    chroma_subsampling_ = chroma_subsampling;
  }
  void set_transfer_function(uint8_t transfer_function) {
    transfer_function_ = transfer_function;
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
  uint8_t profile_;
  uint8_t level_;
  uint8_t bit_depth_;
  uint8_t color_space_;
  uint8_t chroma_subsampling_;
  uint8_t transfer_function_;
  bool video_full_range_flag_;
  std::vector<uint8_t> codec_initialization_data_;

  DISALLOW_COPY_AND_ASSIGN(VPCodecConfiguration);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILTERS_VP_CODEC_CONFIGURATION_H_
