// Copyright 2020 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/ac4_audio_util.h"

#include "packager/base/macros.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/rcheck.h"

namespace shaka {
namespace media {

namespace {

inline bool AC4ByteAlign(size_t cur_bits, BitReader& bit_reader) {
  if (cur_bits % 8) {
    return bit_reader.SkipBits(8 - (cur_bits % 8));
  }
  return true;
}

// Speaker group index
// Bit,      Location
// 0(LSB),   Left/Right pair
// 1,        Centre
// 2,        Left surround/Right surround pair
// 3,        Left back/Right back pair
// 4,        Top front left/Top front right pair
// 5,        Top back left/Top back right pair
// 6,        LFE
// 7,        Top left/Top right pair
// 8,        Top side left/Top side right pair
// 9,        Top front centre
// 10,       Top back centre
// 11,       Top centre
// 12,       LFE2
// 13,       Bottom front left/Bottom front right pair
// 14,       Bottom front centre
// 15,       Back centre
// 16,       Left screen/Right screen pair
// 17,       Left wide/Right wide pair
// 18,       Vertical height left/Vertical height right pair
enum kAC4AudioChannelGroupIndex {
    kLRpair = 0x1,
    kCenter = 0x2,
    kLsRsPair = 0x4,
    kLbRbPair = 0x8,
    kTflTfrPair = 0x10,
    kTblTbrPair = 0x20,
    kLFE = 0x40,
    kTlTrPair = 0x80,
    kTslTsrPair = 0x100,
    kTopfrontCentre = 0x200,
    kTopbackCentre = 0x400,
    kTopCentre = 0x800,
    kLFE2 = 0x1000,
    kBflBfrPair = 0x2000,
    kBottomFrontCentre = 0x4000,
    kBackCentre = 0x8000,
    kLscrRscrPair = 0x10000,
    kLwRw = 0x20000,
    kVhlVhrPair = 0x40000,
};

// Mapping of channel configurations to the MPEG audio value based on ETSI TS
// 103 192-2 V1.2.1 Digital Audio Compression (AC-4) Standard Table G.1
uint32_t AC4ChannelMasktoMpegValue(uint32_t channel_mask) {
  uint32_t ret = 0;

  switch (channel_mask) {
    case kCenter:
      ret = 1;
      break;
    case kLRpair:
      ret = 2;
      break;
    case kCenter | kLRpair:
      ret = 3;
      break;
    case kCenter | kLRpair | kBackCentre:
      ret = 4;
      break;
    case kCenter | kLRpair | kLsRsPair:
      ret = 5;
      break;
    case kCenter | kLRpair | kLsRsPair | kLFE:
      ret = 6;
      break;
    case kCenter | kLRpair | kLsRsPair | kLFE | kLwRw:
      ret = 7;
      break;
    case kBackCentre | kLRpair:
      ret = 9;
      break;
    case kLRpair | kLsRsPair:
      ret = 10;
      break;
    case kCenter | kLRpair | kLsRsPair | kLFE | kBackCentre:
      ret = 11;
      break;
    case kCenter | kLRpair | kLsRsPair | kLbRbPair | kLFE:
      ret = 12;
      break;
    case kLwRw | kBackCentre | kBottomFrontCentre | kBflBfrPair | kLFE2 |
         kTopCentre | kTopbackCentre | kTopfrontCentre | kTslTsrPair | kLFE |
         kTblTbrPair | kTflTfrPair | kLbRbPair | kLsRsPair | kCenter | kLRpair:
    case kVhlVhrPair | kLwRw | kBackCentre | kBottomFrontCentre | kBflBfrPair|
         kLFE2 | kTopCentre | kTopbackCentre | kTopfrontCentre | kTslTsrPair |
         kLFE | kTblTbrPair | kLbRbPair | kLsRsPair | kCenter | kLRpair:
      ret = 13;
      break;
    case kLFE | kTflTfrPair | kLsRsPair | kCenter | kLRpair:
    case kVhlVhrPair | kLFE | kCenter | kLRpair | kLsRsPair:
      ret = 14;
      break;
    case kLFE2 | kTopbackCentre | kLFE | kTflTfrPair | kCenter | kLRpair |
         kLsRsPair | kLbRbPair:
    case kVhlVhrPair | kLFE2 | kTopbackCentre | kLFE | kCenter | kLRpair |
         kLsRsPair | kLbRbPair:
      ret = 15;
      break;
    case kLFE | kTblTbrPair | kTflTfrPair | kLsRsPair | kCenter | kLRpair:
    case kVhlVhrPair | kLFE | kTblTbrPair | kLsRsPair | kCenter | kLRpair:
      ret = 16;
      break;
    case kTopCentre | kTopfrontCentre | kLFE | kTblTbrPair | kTflTfrPair |
         kLsRsPair | kCenter | kLRpair:
    case kVhlVhrPair | kTopCentre | kTopfrontCentre | kLFE | kTblTbrPair |
         kLsRsPair | kCenter | kLRpair:
      ret = 17;
      break;
    case kTopCentre | kTopfrontCentre | kLFE | kTblTbrPair | kTflTfrPair |
         kCenter | kLRpair | kLsRsPair | kLbRbPair:
    case kVhlVhrPair | kTopCentre | kTopfrontCentre | kLFE | kTblTbrPair |
         kCenter | kLRpair | kLsRsPair | kLbRbPair:
      ret = 18;
      break;
    case kLFE | kTblTbrPair | kTflTfrPair | kCenter | kLRpair | kLsRsPair |
         kLbRbPair:
    case kVhlVhrPair | kLFE | kTblTbrPair | kCenter | kLRpair | kLsRsPair |
         kLbRbPair:
      ret = 19;
      break;
    case kLscrRscrPair | kLFE | kTblTbrPair | kTflTfrPair | kCenter | kLRpair |
         kLsRsPair | kLbRbPair:
    case kVhlVhrPair | kLscrRscrPair | kLFE | kTblTbrPair | kCenter | kLRpair |
         kLsRsPair | kLbRbPair:
      ret = 20;
      break;
    default:
      ret = 0xffffffff;
  }

  return ret;
}

// Parse AC-4 substream group based on ETSI TS 103 192-2 V1.2.1
// Digital Audio Compression (AC-4) Standard E.11.
bool ParseAC4SubStreamGroupDsi(BitReader& bit_reader) {
  // TODO: If b_substream_present ==0, is it valid for OTT?
  bool b_substream_present;
  RCHECK(bit_reader.ReadBits(1, &b_substream_present));
  bool b_hsf_ext;
  RCHECK(bit_reader.ReadBits(1, &b_hsf_ext));
  bool b_channel_coded;
  RCHECK(bit_reader.ReadBits(1, &b_channel_coded));
  uint8_t n_substreams = 0;
  RCHECK(bit_reader.ReadBits(8, &n_substreams));
  for (uint8_t i = 0; i < n_substreams; i++) {
    RCHECK(bit_reader.SkipBits(2));
    bool b_substream_bitrate_indicator;
    RCHECK(bit_reader.ReadBits(1, &b_substream_bitrate_indicator));
    if (b_substream_bitrate_indicator) {
      RCHECK(bit_reader.SkipBits(5));
    }
    if (b_channel_coded) {
      RCHECK(bit_reader.SkipBits(24));
    } else {
      bool b_ajoc;
      RCHECK(bit_reader.ReadBits(1, &b_ajoc));
      if (b_ajoc) {
        bool b_static_dmx;
        RCHECK(bit_reader.ReadBits(1, &b_static_dmx));
        if (b_static_dmx == 0) {
          RCHECK(bit_reader.SkipBits(4));
        }
        RCHECK(bit_reader.SkipBits(6));
      }
      RCHECK(bit_reader.SkipBits(4));
    }
  }
  bool b_content_type;
  RCHECK(bit_reader.ReadBits(1, &b_content_type));
  if (b_content_type) {
    RCHECK(bit_reader.SkipBits(3));
    bool b_language_indicator;
    RCHECK(bit_reader.ReadBits(1, &b_language_indicator));
    if (b_language_indicator) {
      uint8_t n_language_tag_bytes;
      RCHECK(bit_reader.ReadBits(6, &n_language_tag_bytes));
      RCHECK(bit_reader.SkipBits(n_language_tag_bytes * 8));
    }
  }
  return true;
}

// Parse AC-4 Presentation based on ETSI TS 103 192-2 V1.2.1
// Digital Audio Compression (AC-4) Standard E.11.
bool ParseAC4PresentationV1Dsi(BitReader& bit_reader,
                               uint8_t& mdcompat,
                               uint32_t& presentation_channel_mask_v1,
                               uint32_t pres_bytes,
                               bool& dolby_atmos_indicator) {
  bool ret = true;
  // Record the initial offset for byte alignment.
  const size_t presentation_start = bit_reader.bit_position();
  uint8_t presentation_config_v1;
  RCHECK(bit_reader.ReadBits(5, &presentation_config_v1));
  uint8_t b_add_emdf_substreams;
  if (presentation_config_v1 == 0x06) {
    b_add_emdf_substreams = 1;
  } else {
    RCHECK(bit_reader.ReadBits(3, &mdcompat));
    bool b_presentation_id;
    RCHECK(bit_reader.ReadBits(1, &b_presentation_id));
    if (b_presentation_id) {
      RCHECK(bit_reader.SkipBits(5));
    }
    RCHECK(bit_reader.SkipBits(19));
    bool b_presentation_channel_coded;
    RCHECK(bit_reader.ReadBits(1, &b_presentation_channel_coded));
    if (b_presentation_channel_coded) {
      uint8_t dsi_presentation_ch_mode;
      RCHECK(bit_reader.ReadBits(5, &dsi_presentation_ch_mode));
      if (dsi_presentation_ch_mode >= 11 && dsi_presentation_ch_mode <= 14) {
        RCHECK(bit_reader.SkipBits(3));
      }
      RCHECK(bit_reader.ReadBits(24, &presentation_channel_mask_v1));
    }
    bool b_presentation_core_differs;
    RCHECK(bit_reader.ReadBits(1, &b_presentation_core_differs));
    if (b_presentation_core_differs) {
      bool b_presentation_core_channel_coded;
      RCHECK(bit_reader.ReadBits(1, &b_presentation_core_channel_coded));
      if (b_presentation_core_channel_coded) {
        RCHECK(bit_reader.SkipBits(2));
      }
    }
    bool b_presentation_filter;
    RCHECK(bit_reader.ReadBits(1, &b_presentation_filter));
    if (b_presentation_filter) {
      RCHECK(bit_reader.SkipBits(1));
      uint8_t n_filter_bytes;
      RCHECK(bit_reader.ReadBits(8, &n_filter_bytes));
      RCHECK(bit_reader.SkipBits(n_filter_bytes * 8));
    }
    if (presentation_config_v1 == 0x1f) {
      ret &= ParseAC4SubStreamGroupDsi(bit_reader);
    } else {
      RCHECK(bit_reader.SkipBits(1));
      if (presentation_config_v1 == 0 ||
          presentation_config_v1 == 1 ||
          presentation_config_v1 == 2) {
        ret &= ParseAC4SubStreamGroupDsi(bit_reader);
        ret &= ParseAC4SubStreamGroupDsi(bit_reader);
      }
      if (presentation_config_v1 == 3 || presentation_config_v1 == 4) {
        ret &= ParseAC4SubStreamGroupDsi(bit_reader);
        ret &= ParseAC4SubStreamGroupDsi(bit_reader);
        ret &= ParseAC4SubStreamGroupDsi(bit_reader);
      }
      if (presentation_config_v1 == 5) {
        uint8_t n_substream_groups_minus2;
        RCHECK(bit_reader.ReadBits(3, &n_substream_groups_minus2));
        for (uint8_t sg = 0; sg < n_substream_groups_minus2 + 2; sg++) {
          ret &= ParseAC4SubStreamGroupDsi(bit_reader);
        }
      }
      if (presentation_config_v1 > 5) {
        uint8_t n_skip_bytes;
        RCHECK(bit_reader.ReadBits(7, &n_skip_bytes));
        RCHECK(bit_reader.SkipBits(n_skip_bytes * 8));
      }
    }
    RCHECK(bit_reader.SkipBits(1));
    RCHECK(bit_reader.ReadBits(1, &b_add_emdf_substreams));
  }
  if (b_add_emdf_substreams) {
    uint8_t n_add_emdf_substreams;
    RCHECK(bit_reader.ReadBits(7, &n_add_emdf_substreams));
    RCHECK(bit_reader.SkipBits(n_add_emdf_substreams * 15));
  }
  bool b_presentation_bitrate_info;
  RCHECK(bit_reader.ReadBits(1, &b_presentation_bitrate_info));
  if (b_presentation_bitrate_info) {
    // Skip bit rate information based on ETSI TS 103 190-2 v1.2.1 E.7.1
    RCHECK(bit_reader.SkipBits(66));
  }
  bool b_alternative;
  RCHECK(bit_reader.ReadBits(1, &b_alternative));
  if (b_alternative) {
    if (!AC4ByteAlign(bit_reader.bit_position() - presentation_start,
                      bit_reader)) {
      return false;
    }
    // Parse alternative infomation based on ETSI TS 103 190-2 v1.2.1 E.12
    uint16_t name_len;
    RCHECK(bit_reader.ReadBits(16, &name_len));
    RCHECK(bit_reader.SkipBits(name_len * 8));
    uint8_t n_targets;
    RCHECK(bit_reader.ReadBits(5, &n_targets));
    RCHECK(bit_reader.SkipBits(n_targets * 11));
  }
  if (!AC4ByteAlign(bit_reader.bit_position() - presentation_start,
                    bit_reader)) {
    return false;
  }
  if ((bit_reader.bit_position() - presentation_start) <=
      (pres_bytes - 1) * 8) {
    RCHECK(bit_reader.SkipBits(1));
    RCHECK(bit_reader.ReadBits(1, &dolby_atmos_indicator));
    RCHECK(bit_reader.SkipBits(4));
    bool b_extended_presentation_group_index;
    RCHECK(bit_reader.ReadBits(1, &b_extended_presentation_group_index));
    if (b_extended_presentation_group_index) {
      RCHECK(bit_reader.SkipBits(9));
    } else {
      RCHECK(bit_reader.SkipBits(1));
    }
  }
  return ret;
}

bool ExtractAc4Data(const std::vector<uint8_t>& ac4_data,
                    uint8_t& bitstream_version,
                    uint8_t& presentation_version,
                    bool& dolby_ims_indicator,
                    uint8_t& mdcompat,
                    uint32_t& presentation_channel_mask_v1,
                    bool& dolby_atmos_indicator) {
  uint16_t n_presentation;
  BitReader bit_reader(ac4_data.data(), ac4_data.size());

  RCHECK(bit_reader.SkipBits(3) && bit_reader.ReadBits(7, &bitstream_version));
  RCHECK(bit_reader.SkipBits(5) && bit_reader.ReadBits(9, &n_presentation));

  if (bitstream_version > 1) {
    uint8_t b_program_id = 0;
    RCHECK(bit_reader.ReadBits(1, &b_program_id));
    if (b_program_id) {
      RCHECK(bit_reader.SkipBits(16));
      uint8_t b_uuid = 0;
      RCHECK(bit_reader.ReadBits(1, &b_uuid));
      if (b_uuid) {
        RCHECK(bit_reader.SkipBits(16 * 8));
      }
    }
  } else if (bitstream_version == 0 || bitstream_version == 1) {
    // Only presentation version 1 and 2 are supported.
    // Bitstream_version == 0 has presentation version of 0,
    // Bitstream_version == 1 has mixed presentation version of 0 and 1.
    LOG(WARNING) << "Bitstream version 0 or 1 is not supported";
    return false;
  } else {
    LOG(WARNING) << "Invaild Bitstream version";
    return false;
  }

  RCHECK(bit_reader.SkipBits(66));

  if (!AC4ByteAlign(bit_reader.bit_position(), bit_reader)) {
    return false;
  }

  dolby_ims_indicator = false;
  dolby_atmos_indicator = false;
  bool atmos_presentation = false;

  // Pre-read presentation_version and skip first ReadBits.
  bit_reader.ReadBits(8, &presentation_version);
  if ((presentation_version == 2 && n_presentation > 2) ||
      (presentation_version == 1 && n_presentation > 1) ) {
    LOG(WARNING) << "Seeing multiple presentations, only single presentation \
                    (including IMS presentation) is supported";
    return false;
  }

  // Logic for future usage if need support multiple presentaions.
  n_presentation = 1;
  for (uint8_t i = 0; i < n_presentation; i++) {
    if (i != 0) {
      bit_reader.ReadBits(8, &presentation_version);
    }
    uint32_t pres_bytes;
    bit_reader.ReadBits(8, &pres_bytes);
    if (pres_bytes == 255) {
      uint32_t add_pres_bytes;
      bit_reader.ReadBits(16, &add_pres_bytes);
      pres_bytes += add_pres_bytes;
    }

    size_t presentation_bits = 0;
    if (presentation_version == 0) {
      // presentation version 0 is not supported, do nothing
      LOG(WARNING) << "Presentation version 0 is not supported";
      return false;
    } else {
      if (presentation_version == 1 || presentation_version == 2) {
        if (presentation_version == 2) {
          dolby_ims_indicator = true;
        }
        const size_t presentation_start = bit_reader.bit_position();
        if (!ParseAC4PresentationV1Dsi(bit_reader,
                                       mdcompat,
                                       presentation_channel_mask_v1,
                                       pres_bytes,
                                       dolby_atmos_indicator)) {
          return false;
        }
        const size_t presentation_end = bit_reader.bit_position();
        presentation_bits = presentation_end - presentation_start;
        if (dolby_atmos_indicator) {
          atmos_presentation = true;
        }
      } else {
        LOG(WARNING) << "Invaild Presentation version";
        return false;
      }
    }
    size_t skip_bits = pres_bytes * 8 - presentation_bits;
    bit_reader.SkipBits(skip_bits);
  }
  if (dolby_ims_indicator) {
    presentation_version = 2;
  }
  if (atmos_presentation) {
    dolby_atmos_indicator = true;
  }
  return true;
}
}  // namespace

bool CalculateAC4ChannelMask(const std::vector<uint8_t>& ac4_data,
                             uint32_t& channel_mask) {
  uint8_t bitstream_version;
  uint8_t presentation_version;
  uint8_t mdcompat;
  bool dolby_ims_indicator;
  uint32_t presentation_channel_mask_v1 = 0;
  bool dolby_atmos_indicator;

  if (!ExtractAc4Data(ac4_data, bitstream_version, presentation_version,
                      dolby_ims_indicator, mdcompat,
                      presentation_channel_mask_v1,
                      dolby_atmos_indicator)) {
    LOG(WARNING) << "Seeing invalid AC4 data: "
                 << base::HexEncode(ac4_data.data(), ac4_data.size());
    return false;
  }

  if (presentation_channel_mask_v1 == 0) {
    channel_mask = 0x800000;
  } else {
    channel_mask = presentation_channel_mask_v1;
  }
  return true;
}

bool CalculateAC4ChannelMpegValue(const std::vector<uint8_t>& ac4_data,
                                  uint32_t& channel_mpeg_value) {
  uint8_t bitstream_version;
  uint8_t presentation_version;
  uint8_t mdcompat;
  bool dolby_ims_indicator;
  uint32_t channel_mask = 0;
  bool dolby_atmos_indicator;

  if (!ExtractAc4Data(ac4_data, bitstream_version, presentation_version,
                      dolby_ims_indicator, mdcompat, channel_mask,
                      dolby_atmos_indicator)) {
    LOG(WARNING) << "Seeing invalid AC4 data: "
                 << base::HexEncode(ac4_data.data(), ac4_data.size());
    return false;
  }

  channel_mpeg_value = AC4ChannelMasktoMpegValue(channel_mask);
  return true;
}

bool GetAc4CodecInfo(const std::vector<uint8_t>& ac4_data,
                     uint8_t& codec_info) {
  uint8_t bitstream_version;
  uint8_t presentation_version;
  uint8_t mdcompat;
  bool dolby_ims_indicator;
  uint32_t channel_mask;
  bool dolby_atmos_indicator;

  if (!ExtractAc4Data(ac4_data, bitstream_version, presentation_version,
                      dolby_ims_indicator, mdcompat, channel_mask,
                      dolby_atmos_indicator)) {
    LOG(WARNING) << "Seeing invalid AC4 data: "
                 << base::HexEncode(ac4_data.data(), ac4_data.size());
    return false;
  }
  // TODO: The only valid value of bitstream_version (8 bits) is 2,
  // presentation_version (8 bits) is 1 or 2, and mdcompat is 3 bits. So uint8_t
  // is fine now. If Dolby extend the range of bitstream_version and
  // presentation_version in future, need change it to uint16_t or uint32_t,
  // and AudioStreamInfo::GetCodecString also need to be changed.
  codec_info = ((bitstream_version << 5) | (presentation_version << 3) |
                mdcompat);
  return true;
}

bool GetAc4ImsInfo(const std::vector<uint8_t>& ac4_data, bool& ims_flag,
                   bool& src_atmos_flag) {
  uint8_t bitstream_version;
  uint8_t presentation_version;
  uint8_t mdcompat;
  uint32_t channel_mask;

  if (!ExtractAc4Data(ac4_data, bitstream_version, presentation_version,
                      ims_flag, mdcompat, channel_mask, src_atmos_flag)) {
    LOG(WARNING) << "Seeing invalid AC4 data: "
                 << base::HexEncode(ac4_data.data(), ac4_data.size());
    return false;
  }

  return true;
}

}  // namespace media
}  // namespace shaka
