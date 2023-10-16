// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_AVC_DECODER_CONFIGURATION_RECORD_H_
#define PACKAGER_MEDIA_CODECS_AVC_DECODER_CONFIGURATION_RECORD_H_

#include <cstdint>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/codecs/decoder_configuration_record.h>

namespace shaka {
namespace media {

/// Class for parsing AVC decoder configuration record.
class AVCDecoderConfigurationRecord : public DecoderConfigurationRecord {
 public:
  AVCDecoderConfigurationRecord();
  ~AVCDecoderConfigurationRecord() override;

  /// @return The codec string.
  std::string GetCodecString(FourCC codec_fourcc) const;

  uint8_t version() const { return version_; }
  uint8_t profile_indication() const { return profile_indication_; }
  uint8_t profile_compatibility() const { return profile_compatibility_; }
  uint8_t avc_level() const { return avc_level_; }
  uint32_t coded_width() const { return coded_width_; }
  uint32_t coded_height() const { return coded_height_; }
  uint32_t pixel_width() const { return pixel_width_; }
  uint32_t pixel_height() const { return pixel_height_; }
  uint8_t chroma_format() const { return chroma_format_; }
  uint8_t bit_depth_luma_minus8() const { return bit_depth_luma_minus8_; }
  uint8_t bit_depth_chroma_minus8() const { return bit_depth_chroma_minus8_; }

  /// Static version of GetCodecString.
  /// @return The codec string.
  static std::string GetCodecString(FourCC codec_fourcc,
                                    uint8_t profile_indication,
                                    uint8_t profile_compatibility,
                                    uint8_t avc_level);

 private:
  bool ParseInternal() override;

  uint8_t version_ = 0;
  uint8_t profile_indication_ = 0;
  uint8_t profile_compatibility_ = 0;
  uint8_t avc_level_ = 0;

  // Extracted from SPS.
  uint32_t coded_width_ = 0;
  uint32_t coded_height_ = 0;
  uint32_t pixel_width_ = 0;
  uint32_t pixel_height_ = 0;
  
  // Only should be present for special case profile values.
  // Refer to ISO/IEC 14496-15 Section 5.3.3.1.1.
  uint8_t chroma_format_ = 0;
  uint8_t bit_depth_luma_minus8_ = 0;
  uint8_t bit_depth_chroma_minus8_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AVCDecoderConfigurationRecord);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_AVC_DECODER_CONFIGURATION_RECORD_H_
