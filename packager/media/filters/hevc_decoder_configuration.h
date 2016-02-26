// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILTERS_HEVC_DECODER_CONFIGURATION_H_
#define MEDIA_FILTERS_HEVC_DECODER_CONFIGURATION_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "packager/base/macros.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/filters/decoder_configuration.h"

namespace edash_packager {
namespace media {

/// Class for parsing HEVC decoder configuration.
class HEVCDecoderConfiguration : public DecoderConfiguration {
 public:
  HEVCDecoderConfiguration();
  ~HEVCDecoderConfiguration() override;

  /// @return The codec string.
  std::string GetCodecString(VideoCodec codec) const;

 private:
  bool ParseInternal() override;

  uint8_t version_;
  uint8_t general_profile_space_;
  bool general_tier_flag_;
  uint8_t general_profile_idc_;
  uint32_t general_profile_compatibility_flags_;
  std::vector<uint8_t> general_constraint_indicator_flags_;
  uint8_t general_level_idc_;

  DISALLOW_COPY_AND_ASSIGN(HEVCDecoderConfiguration);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILTERS_HEVC_DECODER_CONFIGURATION_H_
