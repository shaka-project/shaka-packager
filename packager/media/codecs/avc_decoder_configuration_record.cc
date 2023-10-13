// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/avc_decoder_configuration_record.h>

#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>

#include <packager/macros/logging.h>
#include <packager/media/base/buffer_reader.h>
#include <packager/media/base/rcheck.h>
#include <packager/media/codecs/h264_parser.h>
#include <packager/utils/bytes_to_string_view.h>

namespace shaka {
namespace media {

AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord() = default;

AVCDecoderConfigurationRecord::~AVCDecoderConfigurationRecord() = default;

bool AVCDecoderConfigurationRecord::ParseInternal() {
  // See ISO 14496-15 sec 5.3.3.1.2
  BufferReader reader(data(), data_size());

  RCHECK(reader.Read1(&version_) && version_ == 1 &&
         reader.Read1(&profile_indication_) &&
         reader.Read1(&profile_compatibility_) && reader.Read1(&avc_level_));

  uint8_t length_size_minus_one;
  RCHECK(reader.Read1(&length_size_minus_one));
  if ((length_size_minus_one & 0x3) == 2) {
    LOG(ERROR) << "Invalid NALU length size.";
    return false;
  }
  set_nalu_length_size((length_size_minus_one & 0x3) + 1);

  uint8_t num_sps;
  RCHECK(reader.Read1(&num_sps));
  num_sps &= 0x1f;
  if (num_sps < 1) {
    VLOG(1) << "No SPS found.";
  }

  for (uint8_t i = 0; i < num_sps; i++) {
    uint16_t size = 0;
    RCHECK(reader.Read2(&size));
    const uint8_t* nalu_data = reader.data() + reader.pos();
    RCHECK(reader.SkipBytes(size));

    Nalu nalu;
    RCHECK(nalu.Initialize(Nalu::kH264, nalu_data, size));
    RCHECK(nalu.type() == Nalu::H264_SPS);
    AddNalu(nalu);

    if (i == 0) {
      // It is unlikely to have more than one SPS in practice. Also there's
      // no way to change the {coded,pixel}_{width,height} dynamically from
      // VideoStreamInfo.
      int sps_id = 0;
      H264Parser parser;
      RCHECK(parser.ParseSps(nalu, &sps_id) == H264Parser::kOk);
      set_transfer_characteristics(
          parser.GetSps(sps_id)->transfer_characteristics);
      RCHECK(ExtractResolutionFromSps(*parser.GetSps(sps_id), &coded_width_,
                                      &coded_height_, &pixel_width_,
                                      &pixel_height_));
    }
  }

  uint8_t pps_count;
  RCHECK(reader.Read1(&pps_count));
  for (uint8_t i = 0; i < pps_count; i++) {
    uint16_t size = 0;
    RCHECK(reader.Read2(&size));
    const uint8_t* nalu_data = reader.data() + reader.pos();
    RCHECK(reader.SkipBytes(size));

    Nalu nalu;
    RCHECK(nalu.Initialize(Nalu::kH264, nalu_data, size));
    RCHECK(nalu.type() == Nalu::H264_PPS);
    AddNalu(nalu);
  }

  if (profile_indication_ == 100 || profile_indication_ == 110 || 
      profile_indication_ == 122 || profile_indication_ == 144) {

    uint8_t sps_ext_count;
    if (!reader.Read1(&chroma_format_) || !reader.Read1(&bit_depth_luma_minus8_) ||
        !reader.Read1(&bit_depth_chroma_minus8_) || !reader.Read1(&sps_ext_count)) {
       LOG(WARNING) << "Insufficient bits in bitstream for given AVC profile";
       return true;
    }
    chroma_format_ &= 0x3;
    bit_depth_luma_minus8_ &= 0x7;
    bit_depth_chroma_minus8_ &= 0x7;
    for (uint8_t i = 0; i < sps_ext_count; i++) {
      uint16_t size = 0;
      RCHECK(reader.Read2(&size));
      const uint8_t* nalu_data = reader.data() + reader.pos();
      RCHECK(reader.SkipBytes(size));

      Nalu nalu;
      RCHECK(nalu.Initialize(Nalu::kH264, nalu_data, size));
      RCHECK(nalu.type() == Nalu::H264_SPSExtension);
      AddNalu(nalu);
    } 
  } 	
  return true;
}

std::string AVCDecoderConfigurationRecord::GetCodecString(
    FourCC codec_fourcc) const {
  return GetCodecString(codec_fourcc, profile_indication_,
                        profile_compatibility_, avc_level_);
}

std::string AVCDecoderConfigurationRecord::GetCodecString(
    FourCC codec_fourcc,
    uint8_t profile_indication,
    uint8_t profile_compatibility,
    uint8_t avc_level) {
  const uint8_t bytes[] = {profile_indication, profile_compatibility,
                           avc_level};
  return FourCCToString(codec_fourcc) + "." +
         absl::AsciiStrToLower(absl::BytesToHexString(
             byte_array_to_string_view(bytes, std::size(bytes))));
}

}  // namespace media
}  // namespace shaka
