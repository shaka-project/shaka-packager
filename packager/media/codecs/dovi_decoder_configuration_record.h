// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_DOVI_DECODER_CONFIGURATION_RECORD_H_
#define PACKAGER_MEDIA_CODECS_DOVI_DECODER_CONFIGURATION_RECORD_H_

#include <cstdint>
#include <string>
#include <vector>

#include <packager/media/base/fourccs.h>

namespace shaka {
namespace media {

/// Class for parsing Dolby Vision decoder configuration record.
// Implemented according to Dolby Vision Streams Within the ISO Base Media File
// Format Version 2.0:
// https://www.dolby.com/us/en/technologies/dolby-vision/dolby-vision-bitstreams-within-the-iso-base-media-file-format-v2.0.pdf
// and Dolby Vision Streams within the HTTP Live Streaming format Version 2.0:
// https://www.dolby.com/us/en/technologies/dolby-vision/dolby-vision-streams-within-the-http-live-streaming-format-v2.0.pdf
class DOVIDecoderConfigurationRecord {
 public:
  DOVIDecoderConfigurationRecord() = default;
  ~DOVIDecoderConfigurationRecord() = default;

  /// Parses input to extract decoder configuration record.
  /// @return false if there are parsing errors.
  bool Parse(const std::vector<uint8_t>& data);

  /// @return The codec string in the format defined by RFC6381. It is used in
  ///         DASH and HLS manifests.
  std::string GetCodecString(FourCC codec_fourcc) const;

  /// @return The compatiable brand in the format defined by
  /// https://mp4ra.org/#/brands.
  FourCC GetDoViCompatibleBrand(const uint8_t transfer_characteristics) const;

 private:
  DOVIDecoderConfigurationRecord(const DOVIDecoderConfigurationRecord&) =
      delete;
  DOVIDecoderConfigurationRecord& operator=(
      const DOVIDecoderConfigurationRecord&) = delete;

  uint8_t profile_ = 0;
  uint8_t bl_signal_compatibility_id_ = 0;
  uint8_t level_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_DOVI_DECODER_CONFIGURATION_RECORD_H_
