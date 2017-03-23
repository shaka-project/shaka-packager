// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/hevc_decoder_configuration_record.h"

#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/rcheck.h"

namespace shaka {
namespace media {

namespace {

// ISO/IEC 14496-15:2014 Annex E.
std::string GeneralProfileSpaceAsString(uint8_t general_profile_space) {
  switch (general_profile_space) {
    case 0:
      return "";
    case 1:
      return "A";
    case 2:
      return "B";
    case 3:
      return "C";
    default:
      LOG(WARNING) << "Unexpected general_profile_space "
                   << general_profile_space;
      return "";
  }
}

std::string TrimLeadingZeros(const std::string& str) {
  DCHECK_GT(str.size(), 0u);
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '0') continue;
    return str.substr(i);
  }
  return "0";
}

// Encode the 32 bits input, but in reverse bit order, i.e. bit [31] as the most
// significant bit, followed by, bit [30], and down to bit [0] as the least
// significant bit, where bits [i] for i in the range of 0 to 31, inclusive, are
// specified in ISO/IEC 23008‐2, encoded in hexadecimal (leading zeroes may be
// omitted).
std::string ReverseBitsAndHexEncode(uint32_t x) {
  x = ((x & 0x55555555) << 1) | ((x & 0xAAAAAAAA) >> 1);
  x = ((x & 0x33333333) << 2) | ((x & 0xCCCCCCCC) >> 2);
  x = ((x & 0x0F0F0F0F) << 4) | ((x & 0xF0F0F0F0) >> 4);
  const uint8_t bytes[] = {static_cast<uint8_t>(x & 0xFF),
                           static_cast<uint8_t>((x >> 8) & 0xFF),
                           static_cast<uint8_t>((x >> 16) & 0xFF),
                           static_cast<uint8_t>((x >> 24) & 0xFF)};
  return TrimLeadingZeros(base::HexEncode(bytes, arraysize(bytes)));
}

}  // namespace

HEVCDecoderConfigurationRecord::HEVCDecoderConfigurationRecord()
    : version_(0),
      general_profile_space_(0),
      general_tier_flag_(false),
      general_profile_idc_(0),
      general_profile_compatibility_flags_(0),
      general_level_idc_(0) {}

HEVCDecoderConfigurationRecord::~HEVCDecoderConfigurationRecord() {}

bool HEVCDecoderConfigurationRecord::ParseInternal() {
  BufferReader reader(data(), data_size());

  uint8_t profile_indication = 0;
  uint8_t length_size_minus_one = 0;
  uint8_t num_of_arrays = 0;
  RCHECK(reader.Read1(&version_) && version_ == 1 &&
         reader.Read1(&profile_indication) &&
         reader.Read4(&general_profile_compatibility_flags_) &&
         reader.ReadToVector(&general_constraint_indicator_flags_, 6) &&
         reader.Read1(&general_level_idc_) &&
         reader.SkipBytes(8) &&  // Skip uninterested fields.
         reader.Read1(&length_size_minus_one) &&
         reader.Read1(&num_of_arrays));

  general_profile_space_ = profile_indication >> 6;
  RCHECK(general_profile_space_ <= 3u);
  general_tier_flag_ = ((profile_indication >> 5) & 1) == 1;
  general_profile_idc_ = profile_indication & 0x1f;

  if ((length_size_minus_one & 0x3) == 2) {
    LOG(ERROR) << "Invalid NALU length size.";
    return false;
  }
  set_nalu_length_size((length_size_minus_one & 0x3) + 1);

  for (int i = 0; i < num_of_arrays; i++) {
    uint8_t nal_unit_type;
    uint16_t num_nalus;
    RCHECK(reader.Read1(&nal_unit_type));
    nal_unit_type &= 0x3f;
    RCHECK(reader.Read2(&num_nalus));
    for (int j = 0; j < num_nalus; j++) {
      uint16_t nalu_length;
      RCHECK(reader.Read2(&nalu_length));
      uint64_t nalu_offset = reader.pos();
      RCHECK(reader.SkipBytes(nalu_length));

      Nalu nalu;
      RCHECK(nalu.Initialize(Nalu::kH265, data() + nalu_offset, nalu_length));
      RCHECK(nalu.type() == nal_unit_type);
      AddNalu(nalu);
    }
  }

  // TODO(kqyang): Parse SPS to get resolutions.
  return true;
}

std::string HEVCDecoderConfigurationRecord::GetCodecString(
    FourCC codec_fourcc) const {
  // ISO/IEC 14496-15:2014 Annex E.
  std::vector<std::string> fields;
  fields.push_back(FourCCToString(codec_fourcc));
  fields.push_back(GeneralProfileSpaceAsString(general_profile_space_) +
                   base::IntToString(general_profile_idc_));
  fields.push_back(
      ReverseBitsAndHexEncode(general_profile_compatibility_flags_));
  fields.push_back((general_tier_flag_ ? "H" : "L") +
                   base::IntToString(general_level_idc_));

  // Remove trailing bytes that are zero.
  std::vector<uint8_t> constraints = general_constraint_indicator_flags_;
  size_t size = constraints.size();
  for (; size > 0; --size) {
    if (constraints[size - 1] != 0) break;
  }
  constraints.resize(size);
  for (uint8_t constraint : constraints)
    fields.push_back(TrimLeadingZeros(base::HexEncode(&constraint, 1)));

  return base::JoinString(fields, ".");
}

}  // namespace media
}  // namespace shaka
