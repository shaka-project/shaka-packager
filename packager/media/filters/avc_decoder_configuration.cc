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
      avc_level_(0),
      length_size_(0) {}

AVCDecoderConfiguration::~AVCDecoderConfiguration() {}

bool AVCDecoderConfiguration::Parse(const std::vector<uint8_t>& data) {
  BufferReader reader(data.data(), data.size());
  RCHECK(reader.Read1(&version_) && version_ == 1 &&
         reader.Read1(&profile_indication_) &&
         reader.Read1(&profile_compatibility_) && reader.Read1(&avc_level_));

  uint8_t length_size_minus_one;
  RCHECK(reader.Read1(&length_size_minus_one));
  length_size_ = (length_size_minus_one & 0x3) + 1;

  uint8_t num_sps;
  RCHECK(reader.Read1(&num_sps));
  num_sps &= 0x1f;
  if (num_sps < 1) {
    LOG(ERROR) << "No SPS found.";
    return false;
  }

  uint16_t sps_length = 0;
  RCHECK(reader.Read2(&sps_length));

  H264Parser parser;
  int sps_id = 0;
  Nalu nalu;
  RCHECK(nalu.InitializeFromH264(reader.data() + reader.pos(), sps_length));
  RCHECK(parser.ParseSPS(nalu, &sps_id) == H264Parser::kOk);
  return ExtractResolutionFromSps(*parser.GetSPS(sps_id), &coded_width_,
                                  &coded_height_, &pixel_width_,
                                  &pixel_height_);
  // It is unlikely to have more than one SPS in practice. Also there's
  // no way to change the {coded,pixel}_{width,height} dynamically from
  // VideoStreamInfo. So skip the rest (if there are any).
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
