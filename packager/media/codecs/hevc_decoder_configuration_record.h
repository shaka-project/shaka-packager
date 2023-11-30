// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_HEVC_DECODER_CONFIGURATION_RECORD_H_
#define PACKAGER_MEDIA_CODECS_HEVC_DECODER_CONFIGURATION_RECORD_H_

#include <cstdint>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/codecs/decoder_configuration_record.h>

namespace shaka {
namespace media {

/// Class for parsing HEVC decoder configuration record.
class HEVCDecoderConfigurationRecord : public DecoderConfigurationRecord {
 public:
  HEVCDecoderConfigurationRecord();
  ~HEVCDecoderConfigurationRecord() override;

  /// @return The codec string.
  std::string GetCodecString(FourCC codec_fourcc) const;

 private:
  bool ParseInternal() override;

  uint8_t version_ = 0;
  uint8_t general_profile_space_ = 0;
  bool general_tier_flag_ = false;
  uint8_t general_profile_idc_ = 0;
  uint32_t general_profile_compatibility_flags_ = 0;
  std::vector<uint8_t> general_constraint_indicator_flags_;
  uint8_t general_level_idc_ = 0;

  DISALLOW_COPY_AND_ASSIGN(HEVCDecoderConfigurationRecord);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_HEVC_DECODER_CONFIGURATION_RECORD_H_
