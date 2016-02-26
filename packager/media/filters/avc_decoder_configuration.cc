// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/avc_decoder_configuration.h"

#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/filters/h264_parser.h"
#include "packager/media/formats/mp4/rcheck.h"

namespace edash_packager {
namespace media {

AVCDecoderConfiguration::AVCDecoderConfiguration()
    : version_(0),
      profile_indication_(0),
      profile_compatibility_(0),
      avc_level_(0) {}

AVCDecoderConfiguration::~AVCDecoderConfiguration() {}

bool AVCDecoderConfiguration::ParseInternal() {
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
    LOG(ERROR) << "No SPS found.";
    return false;
  }

  for (uint8_t i = 0; i < num_sps; i++) {
    uint16_t size = 0;
    RCHECK(reader.Read2(&size));
    const uint8_t* nalu_data = reader.data() + reader.pos();
    RCHECK(reader.SkipBytes(size));

    Nalu nalu;
    RCHECK(nalu.InitializeFromH264(nalu_data, size));
    RCHECK(nalu.type() == Nalu::H264_SPS);
    AddNalu(nalu);

    if (i == 0) {
      // It is unlikely to have more than one SPS in practice. Also there's
      // no way to change the {coded,pixel}_{width,height} dynamically from
      // VideoStreamInfo.
      int sps_id = 0;
      H264Parser parser;
      RCHECK(parser.ParseSPS(nalu, &sps_id) == H264Parser::kOk);
      RCHECK(ExtractResolutionFromSps(*parser.GetSPS(sps_id), &coded_width_,
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
    RCHECK(nalu.InitializeFromH264(nalu_data, size));
    RCHECK(nalu.type() == Nalu::H264_PPS);
    AddNalu(nalu);
  }

  return true;
}

std::string AVCDecoderConfiguration::GetCodecString() const {
  return GetCodecString(profile_indication_, profile_compatibility_,
                        avc_level_);
}

std::string AVCDecoderConfiguration::GetCodecString(
    uint8_t profile_indication,
    uint8_t profile_compatibility,
    uint8_t avc_level) {
  const uint8_t bytes[] = {profile_indication, profile_compatibility,
                           avc_level};
  return "avc1." +
         base::StringToLowerASCII(base::HexEncode(bytes, arraysize(bytes)));
}

}  // namespace media
}  // namespace edash_packager
