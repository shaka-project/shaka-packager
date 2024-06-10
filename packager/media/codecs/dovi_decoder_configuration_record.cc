// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/dovi_decoder_configuration_record.h>

#include <absl/strings/str_format.h>

#include <packager/media/base/bit_reader.h>
#include <packager/media/base/rcheck.h>

namespace shaka {
namespace media {

bool DOVIDecoderConfigurationRecord::Parse(const std::vector<uint8_t>& data) {
  BitReader reader(data.data(), data.size());

  // Dolby Vision Streams Within the ISO Base Media File Format Version 2.0:
  // https://www.dolby.com/us/en/technologies/dolby-vision/dolby-vision-bitstreams-within-the-iso-base-media-file-format-v2.0.pdf
  uint8_t major_version = 0;
  uint8_t minor_version = 0;
  RCHECK(reader.ReadBits(8, &major_version) && major_version == 1 &&
         reader.ReadBits(8, &minor_version) && minor_version == 0 &&
         reader.ReadBits(7, &profile_) && reader.ReadBits(6, &level_) &&
         reader.SkipBits(3) &&
         reader.ReadBits(4, &bl_signal_compatibility_id_));
  return true;
}

std::string DOVIDecoderConfigurationRecord::GetCodecString(
    FourCC codec_fourcc) const {
  // Dolby Vision Streams within the HTTP Live Streaming format Version 2.0:
  // https://www.dolby.com/us/en/technologies/dolby-vision/dolby-vision-streams-within-the-http-live-streaming-format-v2.0.pdf
  return absl::StrFormat("%s.%02d.%02d", FourCCToString(codec_fourcc).c_str(),
                         profile_, level_);
}

FourCC DOVIDecoderConfigurationRecord::GetDoViCompatibleBrand(
    const uint8_t transfer_characteristics) const {
  // Dolby Vision Streams within the ISO Base Media File Format Version 2.4:
  switch (bl_signal_compatibility_id_) {
    case 1:
      return FOURCC_db1p;
    case 2:
      return FOURCC_db2g;
    case 4:
      if (transfer_characteristics == 14) {
        return FOURCC_db4g;
      }
      // transfer_characteristics == 18
      return FOURCC_db4h;
    default:
      return FOURCC_NULL;
  }
}

}  // namespace media
}  // namespace shaka
