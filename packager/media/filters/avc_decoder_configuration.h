// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILTERS_AVC_DECODER_CONFIGURATION_H_
#define MEDIA_FILTERS_AVC_DECODER_CONFIGURATION_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "packager/base/macros.h"

namespace edash_packager {
namespace media {

/// Class for parsing AVC decoder configuration.
class AVCDecoderConfiguration {
 public:
  AVCDecoderConfiguration();
  ~AVCDecoderConfiguration();

  /// Parses input to extract AVC decoder configuration data.
  /// @return false if there is parsing errors.
  bool Parse(const std::vector<uint8_t>& data);

  /// @return The codec string.
  std::string GetCodecString() const;

  uint8_t version() const { return version_; }
  uint8_t profile_indication() const { return profile_indication_; }
  uint8_t profile_compatibility() const { return profile_compatibility_; }
  uint8_t avc_level() const { return avc_level_; }
  uint8_t length_size() const { return length_size_; }
  uint32_t coded_width() const { return coded_width_; }
  uint32_t coded_height() const { return coded_height_; }
  uint32_t pixel_width() const { return pixel_width_; }
  uint32_t pixel_height() const { return pixel_height_; }

  /// Static version of GetCodecString.
  /// @return The codec string.
  static std::string GetCodecString(uint8_t profile_indication,
                                    uint8_t profile_compatibility,
                                    uint8_t avc_level);

 private:
  uint8_t version_;
  uint8_t profile_indication_;
  uint8_t profile_compatibility_;
  uint8_t avc_level_;
  uint8_t length_size_;

  // Extracted from SPS.
  uint32_t coded_width_;
  uint32_t coded_height_;
  uint32_t pixel_width_;
  uint32_t pixel_height_;

  DISALLOW_COPY_AND_ASSIGN(AVCDecoderConfiguration);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILTERS_AVC_DECODER_CONFIGURATION_H_
