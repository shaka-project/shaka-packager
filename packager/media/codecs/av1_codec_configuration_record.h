// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_AV1_CODEC_CONFIGURATION_RECORD_H_
#define PACKAGER_MEDIA_CODECS_AV1_CODEC_CONFIGURATION_RECORD_H_

#include <stdint.h>
#include <string>
#include <vector>

namespace shaka {
namespace media {

/// Class for parsing AV1 codec configuration record.
class AV1CodecConfigurationRecord {
 public:
  AV1CodecConfigurationRecord();
  ~AV1CodecConfigurationRecord();

  /// Parses input to extract codec configuration record.
  /// @return false if there are parsing errors.
  bool Parse(const std::vector<uint8_t>& data) {
    return Parse(data.data(), data.size());
  }

  /// Parses input to extract decoder configuration record.
  /// @return false if there are parsing errors.
  bool Parse(const uint8_t* data, size_t data_size);

  /// @return The codec string.
  std::string GetCodecString() const;

  std::string GetCodecString(uint16_t color_primaries,
                             uint16_t transfer_characteristics,
                             uint16_t matrix_coefficients,
                             uint8_t video_full_range_flag) const;

 private:
  int profile_ = 0;
  int level_ = 0;
  int tier_ = 0;
  int bit_depth_ = 0;
  int mono_chrome_ = 0;
  int chroma_subsampling_x_ = 0;
  int chroma_subsampling_y_ = 0;
  int chroma_sample_position_ = 0;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the internal data
  // is small, the performance impact is minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_AV1_CODEC_CONFIGURATION_RECORD_H_
