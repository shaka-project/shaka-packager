// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/ac4_parser.h>

#include <algorithm>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/base/rcheck.h>

using namespace shaka::media;

namespace shaka {
namespace media {
namespace {

// ch_mode - TS 103 190-2 table 78
const int CH_MODE_MONO = 0;
const int CH_MODE_STEREO = 1;
const int CH_MODE_3_0 = 2;
const int CH_MODE_5_0 = 3;
const int CH_MODE_5_1 = 4;
const int CH_MODE_70_34 = 5;
const int CH_MODE_71_34 = 6;
const int CH_MODE_70_52 = 7;
const int CH_MODE_71_52 = 8;
const int CH_MODE_70_322 = 9;
const int CH_MODE_71_322 = 10;
const int CH_MODE_7_0_4 = 11;
const int CH_MODE_7_1_4 = 12;
const int CH_MODE_9_0_4 = 13;
const int CH_MODE_9_1_4 = 14;
const int CH_MODE_22_2 = 15;
const int CH_MODE_RESERVED = 16;

int ReadAc4VariableBits(BitReader* reader, int nBits) {
  int value = 0;
  int extra_value;
  int b_moreBits;
  do {
    RCHECK(reader->ReadBits(nBits, &extra_value));
    value += extra_value;
    RCHECK(reader->ReadBits(1, &b_moreBits));
    if (b_moreBits == 1) {
      value <<= nBits;
      value += (1 << nBits);
    }
  } while (b_moreBits == 1);
  return value;
}

}  // namespace

AC4Parser::AC4Parser() = default;
AC4Parser::~AC4Parser() = default;

bool AC4Parser::Parse(const uint8_t* data,
                      size_t data_size) {

  BitReader reader(data, data_size);
  if (!ParseAc4Toc(&reader))
    return false;
  return true;
}

bool AC4Parser::ParseAc4Toc(BitReader* reader) {
  // Ac4Toc ac4_toc;
  toc_size = reader->bit_position();
  RCHECK(reader->ReadBits(2, &ac4_toc.bitstream_version));
  if (ac4_toc.bitstream_version == 3) {
    ac4_toc.bitstream_version = ReadAc4VariableBits(reader, 2);
  }
  RCHECK(reader->ReadBits(10, &ac4_toc.sequence_counter));
  RCHECK(reader->ReadBits(1, &ac4_toc.b_wait_frames));
  if (ac4_toc.b_wait_frames) {
    RCHECK(reader->ReadBits(3, &ac4_toc.wait_frames));
    if (ac4_toc.wait_frames > 0) {
      RCHECK(reader->ReadBits(2, &ac4_toc.br_code));
    }
  }
  RCHECK(reader->ReadBits(1, &ac4_toc.fs_index));
  RCHECK(reader->ReadBits(4, &ac4_toc.frame_rate_index));
  RCHECK(reader->ReadBits(1, &ac4_toc.b_iframe_global));
  RCHECK(reader->ReadBits(1, &ac4_toc.b_single_presentation));
  if (ac4_toc.b_single_presentation) {
    ac4_toc.n_presentations = 1;
  } else {
    RCHECK(reader->ReadBits(1, &ac4_toc.b_more_presentations));
    if (ac4_toc.b_more_presentations) {
      ac4_toc.n_presentations = ReadAc4VariableBits(reader, 2) + 2;
    } else {
      ac4_toc.n_presentations = 0;
    }
  }
  int payload_base = 0;
  RCHECK(reader->ReadBits(1, &ac4_toc.b_payload_base));
  if (ac4_toc.b_payload_base) {
    RCHECK(reader->ReadBits(5, &ac4_toc.payload_base_minus1));
    payload_base = ac4_toc.payload_base_minus1 + 1;
    if (payload_base == 0x20) {
      payload_base = ReadAc4VariableBits(reader, 3);
    }
  }
  if (ac4_toc.bitstream_version <= 1) {
    printf("Warning: Bitstream version 0 is deprecated\n");
  } else {
    RCHECK(reader->ReadBits(1, &ac4_toc.b_program_id));
    if (ac4_toc.b_program_id) {
      RCHECK(reader->ReadBits(16, &ac4_toc.short_program_id));
      RCHECK(reader->ReadBits(1, &ac4_toc.b_program_uuid_present));
      if (ac4_toc.b_program_uuid_present) {
        for (int cnt = 0; cnt < 16; cnt++) {
          RCHECK(reader->ReadBits(8, &ac4_toc.program_uuid));
        }
      }
    }
    int max_group_index = 0;
    ac4_toc.presentation_v1_infos = 
      new Ac4PresentationV1Info[ac4_toc.n_presentations];
    for (int i = 0; i < ac4_toc.n_presentations; i++) {
      ParseAc4PresentationV1Info(reader, ac4_toc.presentation_v1_infos[i],
                                 max_group_index);
    }
    ac4_toc.total_n_substream_groups = max_group_index + 1;
    ac4_toc.substream_group_infos =
        new Ac4SubstreamGroupInfo[ac4_toc.total_n_substream_groups];
    for (int j = 0; j < ac4_toc.total_n_substream_groups; j++) {
      ParseAc4SubstreamGroupInfo(reader, ac4_toc.substream_group_infos[j], j);
    }
  }

  // ts_10319001v010301p (ETSI TS 103190-1 v1.3.1)
  int n_substreams = 0;
  RCHECK(reader->ReadBits(2, &n_substreams));
  if (n_substreams == 0) {
    n_substreams = ReadAc4VariableBits(reader, 2) + 4;
  }
  int b_size_present = 0;
  if (n_substreams == 1) {
    RCHECK(reader->ReadBits(1, &b_size_present));
  } else {
    b_size_present = 1;
  }
  if (b_size_present) {
    for (int s = 0; s < n_substreams; s ++) {
      int b_more_bits = 0;
      RCHECK(reader->ReadBits(1, &b_more_bits));
      int substream_size = 0;
      RCHECK(reader->ReadBits(10, &substream_size));
      if (b_more_bits) {
        substream_size += (ReadAc4VariableBits(reader, 2) << 10);
      }
    }
  }
  toc_size = reader->bit_position() - toc_size;

  return true;
}

bool AC4Parser::ParseAc4PresentationV1Info(BitReader* reader,
                                           Ac4PresentationV1Info& ac4_presentation_v1_info,
                                           int& max_group_index) {
  int group_index = 0;
  RCHECK(
      reader->ReadBits(1, &ac4_presentation_v1_info.b_single_substream_group));
  if (ac4_presentation_v1_info.b_single_substream_group != 1) {
    RCHECK(reader->ReadBits(3, &ac4_presentation_v1_info.presentation_config));
    if (ac4_presentation_v1_info.presentation_config == 7) {
      ac4_presentation_v1_info.presentation_config =
          ReadAc4VariableBits(reader, 2);
    }
  }
  if (ac4_toc.bitstream_version != 1) {
    // presentation_version();
    //int presentation_version = 0;
    int more_bits;
    while (reader->ReadBits(1, &more_bits) && more_bits) {
      ac4_presentation_v1_info.presentation_version++;
    }
  }
  if (ac4_presentation_v1_info.b_single_substream_group != 1 &&
      ac4_presentation_v1_info.presentation_config == 6) {
    ac4_presentation_v1_info.b_add_emdf_substreams = 1;
  } else {
    if (ac4_toc.bitstream_version != 1) {
      RCHECK(reader->ReadBits(3, &ac4_presentation_v1_info.mdcompat));
    }
    RCHECK(reader->ReadBits(1, &ac4_presentation_v1_info.b_presentation_id));
    if (ac4_presentation_v1_info.b_presentation_id) {
      ac4_presentation_v1_info.b_presentation_id =
          ReadAc4VariableBits(reader, 2);
    }
    ParseFrameRateMultiplyInfo(reader);
    ParseFrameRateFractionsInfo(reader);
    ParseEmdfInfo(reader);

    RCHECK(reader->ReadBits(1, &ac4_presentation_v1_info.b_presentation_filter));

    if (ac4_presentation_v1_info.b_presentation_filter) {
      RCHECK(reader->ReadBits(
          1, &ac4_presentation_v1_info.b_enable_presentation));
    }
    if (ac4_presentation_v1_info.b_single_substream_group == 1) {
      group_index = ParseAc4SgiSpecifier(reader);
      max_group_index = group_index > max_group_index ? group_index:max_group_index;
      ac4_presentation_v1_info.n_substream_groups = 1;
    } else {
      reader->ReadBits(1, &ac4_presentation_v1_info.b_multi_pid);
      switch (ac4_presentation_v1_info.presentation_config) {
        case 0:
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          ac4_presentation_v1_info.n_substream_groups = 2;
          break;
        case 1:
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          ac4_presentation_v1_info.n_substream_groups = 1;
          break;
        case 2:
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          ac4_presentation_v1_info.n_substream_groups = 2;
          break;
        case 3:
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          ac4_presentation_v1_info.n_substream_groups = 3;
          break;
        case 4:
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          group_index = ParseAc4SgiSpecifier(reader);
          max_group_index =
              group_index > max_group_index ? group_index : max_group_index;
          ac4_presentation_v1_info.n_substream_groups = 2;
          break;
        case 5:
          RCHECK(reader->ReadBits(
              2, &ac4_presentation_v1_info.n_substream_groups_minus2));
          ac4_presentation_v1_info.n_substream_groups =
              ac4_presentation_v1_info.n_substream_groups_minus2 + 2;
          if (ac4_presentation_v1_info.n_substream_groups == 5) {
            ac4_presentation_v1_info.n_substream_groups +=
                ReadAc4VariableBits(reader, 2);
          }
          for (int sg = 0; sg < ac4_presentation_v1_info.n_substream_groups;
               sg++) {
            group_index = ParseAc4SgiSpecifier(reader);
            max_group_index =
                group_index > max_group_index ? group_index : max_group_index;
          }
          break;
        default:
          ParsePresentationConfigExtInfo(reader);
          break;
      }
    }
    RCHECK(reader->ReadBits(1, &ac4_presentation_v1_info.b_pre_virtualized));
    RCHECK(reader->ReadBits(1, &ac4_presentation_v1_info.b_add_emdf_substreams));
    ParseAc4PresentationSubstreamInfo(reader);
  }
  if (ac4_presentation_v1_info.b_add_emdf_substreams) {
    RCHECK(reader->ReadBits(2, &ac4_presentation_v1_info.n_add_emdf_substreams));
    if (ac4_presentation_v1_info.n_add_emdf_substreams == 0) {
      ac4_presentation_v1_info.n_add_emdf_substreams =
          ReadAc4VariableBits(reader, 2) + 4;
    }
    for (int i = 0; i < ac4_presentation_v1_info.n_add_emdf_substreams; i ++) {
      ParseEmdfInfo(reader);
    }
  }
  return true;
}

bool AC4Parser::ParseFrameRateMultiplyInfo(BitReader* reader) {
  FrameRateMultiplyInfo frame_rate_multiply_info;
  switch (ac4_toc.frame_rate_index) {
    case 2:
    case 3:
    case 4:
      RCHECK(reader->ReadBits(1, &frame_rate_multiply_info.b_multiplier));
      if (frame_rate_multiply_info.b_multiplier) {  // b_multiplier
        RCHECK(reader->ReadBits(
            1, &frame_rate_multiply_info.multiplier_bit));  // multiplier_bit
      }
      break;
    case 0:
    case 1:
    case 7:
    case 8:
    case 9:
      RCHECK(reader->ReadBits(1, &frame_rate_multiply_info.b_multiplier));
      break;
    default:
      break;
  }
  return true;
}

bool AC4Parser::ParseFrameRateFractionsInfo(BitReader* reader) {
  FrameRateFractionsInfo frame_rate_fractions_info;
  if (ac4_toc.frame_rate_index >= 5 && ac4_toc.frame_rate_index <= 9) {
    RCHECK(
        reader->ReadBits(1, &frame_rate_fractions_info.b_frame_rate_fraction));
  }
  if (ac4_toc.frame_rate_index >= 10 && ac4_toc.frame_rate_index <= 12) {
    RCHECK(
        reader->ReadBits(1, &frame_rate_fractions_info.b_frame_rate_fraction));
    if (frame_rate_fractions_info.b_frame_rate_fraction == 1) {
      RCHECK(reader->ReadBits(
          1, &frame_rate_fractions_info.b_frame_rate_fraction_is_4));
    }
  }
  return true;
}

bool AC4Parser::ParseEmdfInfo(BitReader* reader) {
  EmdfInfo emdf_info;
  RCHECK(reader->ReadBits(2, &emdf_info.emdf_version));
  if (emdf_info.emdf_version == 3) {
    emdf_info.emdf_version += ReadAc4VariableBits(reader, 2);
  }
  RCHECK(reader->ReadBits(3, &emdf_info.key_id));
  if (emdf_info.key_id == 7) {
    emdf_info.key_id += ReadAc4VariableBits(reader, 3);
  }
  RCHECK(reader->ReadBits(1, &emdf_info.b_emdf_payloads_substream_info));
  if (emdf_info.b_emdf_payloads_substream_info) {
    RCHECK(reader->ReadBits(2, &emdf_info.substream_index));
    if (emdf_info.substream_index == 3) {
      emdf_info.substream_index += ReadAc4VariableBits(reader, 2);
    }
  }
  RCHECK(reader->ReadBits(2, &emdf_info.protection_length_primary));
  RCHECK(reader->ReadBits(2, &emdf_info.protection_length_secondary));
  switch (emdf_info.protection_length_primary) {
    case 1:
      RCHECK(reader->ReadBits(8, &emdf_info.protection_bits_primary[0]));
      break;
    case 2:
      for (unsigned idx = 0; idx < 4; idx++) {
        RCHECK(reader->ReadBits(8, &emdf_info.protection_bits_primary[idx]));
      }
      break;
    case 3:
      for (unsigned idx = 0; idx < 16; idx++) {
        RCHECK(reader->ReadBits(8, &emdf_info.protection_bits_primary[idx]));
      }
      break;
    default:
      LOG(ERROR) << "Invalid EMDF primary protection length: "
                 << static_cast<int>(emdf_info.protection_length_primary);
      return false;
      break;
  }
  // protection_bits_secondary
  switch (emdf_info.protection_length_secondary) {
    case 0:
      break;
    case 1:
      RCHECK(reader->ReadBits(8, &emdf_info.protection_bits_secondary[0]));
      break;
    case 2:
      for (unsigned idx = 0; idx < 4; idx++) {
        RCHECK(reader->ReadBits(8, &emdf_info.protection_bits_secondary[idx]));
      }
      break;
    case 3:
      for (unsigned idx = 0; idx < 16; idx++) {
        RCHECK(reader->ReadBits(8, &emdf_info.protection_bits_secondary[idx]));
      }
      break;
    default:
      LOG(ERROR) << "Invalid EMDF secondary protection length: "
                 << static_cast<int>(emdf_info.protection_length_secondary);
      return false;
  }
  return true;
}

bool AC4Parser::ParseAc4PresentationSubstreamInfo(BitReader* reader) {
  Ac4PresentationV1Info ac4_presentation_v1_info;
  RCHECK(reader->ReadBits(1, &ac4_presentation_v1_info.b_alternative));
  RCHECK(reader->ReadBits(1, &ac4_presentation_v1_info.b_pres_ndot));
  RCHECK(reader->ReadBits(2, &ac4_presentation_v1_info.substream_index));
  if (ac4_presentation_v1_info.substream_index == 3) {
    ac4_presentation_v1_info.substream_index += ReadAc4VariableBits(reader, 2);
  }
  return true;
}

int AC4Parser::ParseAc4SgiSpecifier(BitReader* reader) {
  int group_index = 0;
  if (ac4_toc.bitstream_version == 1) {
    // ac4_substream_group_info();
  } else {
    RCHECK(reader->ReadBits(3, &group_index));
    if (group_index == 7) {
      group_index += ReadAc4VariableBits(reader, 2);
    }
  }
  return group_index;
}

bool AC4Parser::ParsePresentationConfigExtInfo(BitReader* reader) {
  int n_skip_bytes;
  int b_more_skip_bytes;
  RCHECK(reader->ReadBits(1, &n_skip_bytes));
  RCHECK(reader->ReadBits(1, &b_more_skip_bytes));
  if (b_more_skip_bytes) {
    n_skip_bytes += ReadAc4VariableBits(reader, 2) << 5;
  }
  for (int i = 0; i < n_skip_bytes; i ++) {
    reader->SkipBytes(8);
  }
  return true;
}

int AC4Parser::GetPresentationIdx(int substream_group_index) {
  for (int idx = 0; idx < ac4_toc.n_presentations; idx++) {
    for (int sg = 0; sg < ac4_toc.presentation_v1_infos[idx].n_substream_groups; sg++) {
      if (substream_group_index ==
          ac4_toc.presentation_v1_infos[idx].group_index[sg]) {
        return idx;
      }
    }
  }
  return 0;
}

int AC4Parser::GetPresentationVersion(int substream_group_index) {
  for (int idx = 0; idx < ac4_toc.n_presentations; idx++) {
    for (int sg = 0; sg < ac4_toc.presentation_v1_infos[idx].n_substream_groups;
         sg++) {
      if (substream_group_index ==
          ac4_toc.presentation_v1_infos[idx].group_index[sg]) {
        return ac4_toc.presentation_v1_infos[idx].presentation_version;
      }
    }
  }
  return 0;
}

bool AC4Parser::ParseAc4SubstreamGroupInfo(BitReader* reader,
                                           Ac4SubstreamGroupInfo& ac4_substream_group_info,
                                           int substream_group_index) {
  RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_substreams_present));
  RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_hsf_ext));
  RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_single_substream));
  if (ac4_substream_group_info.b_single_substream) {
    ac4_substream_group_info.n_lf_substreams = 1;
  } else {
    RCHECK(reader->ReadBits(2, &ac4_substream_group_info.n_lf_substreams_minus2));
    ac4_substream_group_info.n_lf_substreams =
        ac4_substream_group_info.n_lf_substreams_minus2 + 2;
    if (ac4_substream_group_info.n_lf_substreams == 5) {
      ac4_substream_group_info.n_lf_substreams += ReadAc4VariableBits(reader, 2);
    }
  }
  RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_channel_coded));
  // find the presentation of current substream group
  int pre_idx = GetPresentationIdx(substream_group_index);
  int frame_rate_factor =
      (ac4_toc.presentation_v1_infos[pre_idx]
           .frame_rate_multiply_info.dsi_frame_rate_multiply_info == 0)
          ? 1
          : (ac4_toc.presentation_v1_infos[pre_idx]
                 .frame_rate_multiply_info.dsi_frame_rate_multiply_info *
             2);
  if (ac4_substream_group_info.b_channel_coded) {
    for (int sus = 0; sus < ac4_substream_group_info.n_lf_substreams; sus ++) {
      if (ac4_toc.bitstream_version == 1) {
        // RCHECK(reader->ReadBits(1, &ac4_substream_group_info.sus_ver)));
      } else {
        ac4_substream_group_info.sus_ver = 1;
      }
      ParseAc4SubstreamInfoChan(reader, GetPresentationVersion(substream_group_index),
                                ac4_toc.fs_index, frame_rate_factor,
                                ac4_substream_group_info.b_substreams_present);
      if (ac4_substream_group_info.b_hsf_ext) {
        ParseAc4HsfExtSubstreamInfo(reader, ac4_substream_group_info);
      }
    }
  } else {
    RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_oamd_substream));
    if (ac4_substream_group_info.b_oamd_substream) {
      ParseOamdSubstreamInfo(reader, ac4_substream_group_info);
    }
    for (int sus = 0; sus < ac4_substream_group_info.n_lf_substreams; sus++) {
      RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_ajoc));
      if (ac4_substream_group_info.b_ajoc) {
        ParseAc4SubstreamInfoAjoc(reader, ac4_toc.fs_index, frame_rate_factor,
                                  ac4_substream_group_info.b_substreams_present);
        if (ac4_substream_group_info.b_hsf_ext) {
            ParseAc4HsfExtSubstreamInfo(reader, ac4_substream_group_info);
        }
      } else {
        ParseAc4SubstreamInfoObj(reader, ac4_toc.fs_index, frame_rate_factor,
                               ac4_substream_group_info.b_substreams_present);
        if (ac4_substream_group_info.b_hsf_ext) {
          ParseAc4HsfExtSubstreamInfo(reader, ac4_substream_group_info);
        }
      }
    }
  }
  RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_content_type));
  if (ac4_substream_group_info.b_content_type) {
    ParseContentType(reader, ac4_substream_group_info);
  }
  return true;
}

bool AC4Parser::ParseContentType(BitReader* reader,
                                 Ac4SubstreamGroupInfo& ac4_substream_group_info) {
  RCHECK(reader->ReadBits(3, &ac4_substream_group_info.content_classifier));
  RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_language_indicator));
  if (ac4_substream_group_info.b_language_indicator) {
    RCHECK(reader->ReadBits(
        1, &ac4_substream_group_info.b_serialized_language_tag));
    if (ac4_substream_group_info.b_serialized_language_tag) {
      RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_start_tag));
      RCHECK(reader->ReadBits(16, &ac4_substream_group_info.language_tag_chunk));
    } else {
      RCHECK(reader->ReadBits(6, &ac4_substream_group_info.n_language_tag_bytes));
      for (int i = 0; i < ac4_substream_group_info.n_language_tag_bytes; i ++) {
        RCHECK(reader->ReadBits(
            8, &ac4_substream_group_info.language_tag_bytes[i]));
      }
    }
  }
  return true;
}

bool AC4Parser::ParseOamdSubstreamInfo(BitReader* reader,
                                       Ac4SubstreamGroupInfo& ac4_substream_group_info) {
  RCHECK(reader->ReadBits(1, &ac4_substream_group_info.b_oamd_ndot));
  if (ac4_substream_group_info.b_substreams_present == 1) {
    RCHECK(reader->ReadBits(2, &ac4_substream_group_info.substream_index));
    if (ac4_substream_group_info.substream_index == 3) {
        ac4_substream_group_info.substream_index +=
            ReadAc4VariableBits(reader, 2);
    }
  }
  return true;
}

bool AC4Parser::ParseAc4HsfExtSubstreamInfo(BitReader* reader,
                                            Ac4SubstreamGroupInfo& ac4_substream_group_info) {
  if (ac4_substream_group_info.b_substreams_present == 1) {
    RCHECK(reader->ReadBits(2, &ac4_substream_group_info.substream_index));
    if (ac4_substream_group_info.substream_index == 3) {
        ac4_substream_group_info.substream_index +=
            ReadAc4VariableBits(reader, 2);
    }
  }
  return true;
}

bool AC4Parser::ParseAc4SubstreamInfoChan(BitReader* reader,
                                          int presentation_version,
                                          int fs_index,
                                          int frame_rate_factor,
                                          int b_substreams_present) {
  Ac4SubstreamInfoChan ac4_substream_info_chan;
  ac4_substream_info_chan.channel_mode = ParseChannelMode(reader, presentation_version);
  if (ac4_substream_info_chan.channel_mode == 0b111111111) {
    ac4_substream_info_chan.channel_mode += ReadAc4VariableBits(reader, 2);
  }
  if ((ac4_substream_info_chan.channel_mode >= CH_MODE_7_0_4) &&
      (ac4_substream_info_chan.channel_mode <= CH_MODE_9_1_4)) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_chan.b_4_back_channels_present));
    RCHECK(reader->ReadBits(1, &ac4_substream_info_chan.b_centre_present));
    RCHECK(reader->ReadBits(2, &ac4_substream_info_chan.top_channels_present));
  }
  if (fs_index == 1) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_chan.b_sf_multiplier));
    if (ac4_substream_info_chan.b_sf_multiplier) {
      RCHECK(reader->ReadBits(1, &ac4_substream_info_chan.sf_multiplier));
    }
  }
  RCHECK(reader->ReadBits(1, &ac4_substream_info_chan.b_bitrate_info));
  if (ac4_substream_info_chan.b_bitrate_info) {
    RCHECK(reader->ReadBits(3, &ac4_substream_info_chan.bitrate_indicator));
    if ((ac4_substream_info_chan.bitrate_indicator & 0x1) == 1) {
      int more_bits = 0;
      RCHECK(reader->ReadBits(2, &more_bits));
      ac4_substream_info_chan.bitrate_indicator =
          (ac4_substream_info_chan.bitrate_indicator << 2) + more_bits;
    }
  }
  if (ac4_substream_info_chan.channel_mode >= CH_MODE_70_52 &&
      ac4_substream_info_chan.channel_mode <= CH_MODE_71_322) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_chan.add_ch_base));
  }
  for (int i = 0; i < frame_rate_factor; i ++) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_chan.b_audio_ndot));
  }
  if (b_substreams_present == 1) {
    RCHECK(reader->ReadBits(2, &ac4_substream_info_chan.substream_index));
    if (ac4_substream_info_chan.substream_index == 3) {
      ac4_substream_info_chan.substream_index += ReadAc4VariableBits(reader, 2);
    }
  }
  return true;
}

bool AC4Parser::ParseAc4SubstreamInfoAjoc(BitReader* reader,
                                          int fs_index,
                                          int frame_rate_factor,
                                          int b_substreams_present) {
  Ac4SubstreamInfoAjoc ac4_substream_info_ajoc;
  RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_lfe));
  RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_static_dmx));
  if (ac4_substream_info_ajoc.b_static_dmx) {
    ac4_substream_info_ajoc.n_fullband_dmx_signals = 5;
  } else {
    RCHECK(reader->ReadBits(4, &ac4_substream_info_ajoc.n_fullband_dmx_signals_minus1));
    ac4_substream_info_ajoc.n_fullband_dmx_signals =
        ac4_substream_info_ajoc.n_fullband_dmx_signals_minus1 + 1;
    ParseBedDynObjAssignment(reader,
                             ac4_substream_info_ajoc.n_fullband_dmx_signals,
                             ac4_substream_info_ajoc);
  }
  RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_oamd_common_data_present));
  if (ac4_substream_info_ajoc.b_oamd_common_data_present) {
    ParseOamdCommonData(reader);
  }
  RCHECK(reader->ReadBits(4, &ac4_substream_info_ajoc.n_fullband_upmix_signals_minus1));
  if (ac4_substream_info_ajoc.n_fullband_upmix_signals_minus1 + 1 == 16) {
    ac4_substream_info_ajoc.n_fullband_upmix_signals_minus1 +=
        ReadAc4VariableBits(reader, 3);
  }
  ParseBedDynObjAssignment(reader,
                           ac4_substream_info_ajoc.n_fullband_upmix_signals_minus1 + 1,
                           ac4_substream_info_ajoc);
  if (fs_index == 1) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_sf_multiplier));
    if (ac4_substream_info_ajoc.b_sf_multiplier) {
      RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.sf_multiplier));
    }
  }
  RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_bitrate_info));
  if (ac4_substream_info_ajoc.b_bitrate_info) {
    RCHECK(reader->ReadBits(3, &ac4_substream_info_ajoc.bitrate_indicator));
    if ((ac4_substream_info_ajoc.bitrate_indicator & 0x1) == 1) {
      int more_bits = 0;
      RCHECK(reader->ReadBits(2, &more_bits));
      ac4_substream_info_ajoc.bitrate_indicator =
          (ac4_substream_info_ajoc.bitrate_indicator << 2) + more_bits;
    }
  }
  for (int i = 0; i < frame_rate_factor; i ++) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_audio_ndot));
  }
  if (b_substreams_present == 1) {
    RCHECK(reader->ReadBits(2, &ac4_substream_info_ajoc.substream_index));
    if (ac4_substream_info_ajoc.substream_index == 3) {
      ac4_substream_info_ajoc.substream_index += ReadAc4VariableBits(reader, 2);
    }
  }
  // sus_ver = 1;
  return true;
}

bool AC4Parser::ParseBedDynObjAssignment(BitReader* reader,
                                         int n_signals,
                                         Ac4SubstreamInfoAjoc& ac4_substream_info_ajoc) {
  RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_dyn_objects_only));
  if (ac4_substream_info_ajoc.b_dyn_objects_only == 0) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_isf));
    if (ac4_substream_info_ajoc.b_isf) {
      RCHECK(reader->ReadBits(3, &ac4_substream_info_ajoc.isf_config));
    } else {
      RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_ch_assign_code));
      if (ac4_substream_info_ajoc.b_ch_assign_code) {
        RCHECK(reader->ReadBits(3, &ac4_substream_info_ajoc.bed_chan_assign_code));
      } else {
        RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_chan_assign_mask));
        if (ac4_substream_info_ajoc.b_chan_assign_mask) {
          RCHECK(reader->ReadBits(1, &ac4_substream_info_ajoc.b_nonstd_bed_channel_assignment));
          if (ac4_substream_info_ajoc.b_nonstd_bed_channel_assignment) {
            RCHECK(reader->ReadBits(17, &ac4_substream_info_ajoc.nonstd_bed_channel_assignment_mask));
          } else {
            RCHECK(reader->ReadBits(10, &ac4_substream_info_ajoc.std_bed_channel_assignment_mask));
          }
        }
        else {
          if (n_signals > 1) {
            int bed_ch_bits = (int)ceil(log((float)n_signals) / log((float)2));
            RCHECK(reader->ReadBits(bed_ch_bits, &ac4_substream_info_ajoc.n_bed_signals_minus1));
            ac4_substream_info_ajoc.n_bed_signals =
                ac4_substream_info_ajoc.n_bed_signals_minus1 + 1;
          } else {
            ac4_substream_info_ajoc.n_bed_signals = 1;
          }
          for (int b = 0; b < ac4_substream_info_ajoc.n_bed_signals; b ++) {
            RCHECK(reader->ReadBits(4, &ac4_substream_info_ajoc.nonstd_bed_channel_assignment));
          }
        }
      }
    }
  }
  return true;
}

bool AC4Parser::ParseOamdCommonData(BitReader* reader) {
  OamdCommonData oamd_common_data;
  RCHECK(reader->ReadBits(1, &oamd_common_data.b_default_screen_size_ratio));
  if (oamd_common_data.b_default_screen_size_ratio == 0) {
    RCHECK(reader->ReadBits(5, &oamd_common_data.master_screen_size_ratio_code));
  }
  RCHECK(reader->ReadBits(1, &oamd_common_data.b_bed_object_chan_distribute));
  RCHECK(reader->ReadBits(1, &oamd_common_data.b_additional_data));
  if (oamd_common_data.b_additional_data) {
    RCHECK(reader->ReadBits(1, &oamd_common_data.add_data_bytes_minus1));
    oamd_common_data.add_data_bytes =
        oamd_common_data.add_data_bytes_minus1 + 1;
    if (oamd_common_data.add_data_bytes == 2) {
      oamd_common_data.add_data_bytes += ReadAc4VariableBits(reader, 2);
    }
    reader->SkipBytes(oamd_common_data.add_data_bytes);
  }
  return true;
}

bool AC4Parser::ParseAc4SubstreamInfoObj(BitReader* reader,
                                         int fs_index,
                                         int frame_rate_factor,
                                         int b_substreams_present) {
  Ac4SubstreamInfoObj ac4_substream_info_obj;
  RCHECK(reader->ReadBits(3, &ac4_substream_info_obj.n_objects_code));
  RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_dynamic_objects));
  if (ac4_substream_info_obj.b_dynamic_objects) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_lfe));
  } else {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_bed_objects));
    if (ac4_substream_info_obj.b_bed_objects) {
      RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_bed_start));
      if (ac4_substream_info_obj.b_bed_start) {
        RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_ch_assign_code));
        if (ac4_substream_info_obj.b_ch_assign_code) {
          RCHECK(reader->ReadBits(3, &ac4_substream_info_obj.bed_chan_assign_code));
        } else {
          RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_nonstd_bed_channel_assignment));
          if (ac4_substream_info_obj.b_nonstd_bed_channel_assignment) {
            RCHECK(reader->ReadBits(17, &ac4_substream_info_obj.nonstd_bed_channel_assignment_mask));
          } else {
            RCHECK(reader->ReadBits(10, &ac4_substream_info_obj.std_bed_channel_assignment_mask));
          }
        }
      }
    } else {
      RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_isf));
      if (ac4_substream_info_obj.b_isf) {
        RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_isf_start));
        if (ac4_substream_info_obj.b_isf_start) {
          RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.isf_config));
        }
      } else {
        int res_bytes = 0;
        RCHECK(reader->ReadBits(4, &res_bytes));
        reader->SkipBits(res_bytes * 8);
      }
    }
  }
  if (fs_index == 1) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_sf_multiplier));
    if (ac4_substream_info_obj.b_sf_multiplier) {
      RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.sf_multiplier));
    }
  }
  RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_bitrate_info));
  if (ac4_substream_info_obj.b_bitrate_info) {
    RCHECK(reader->ReadBits(3, &ac4_substream_info_obj.bitrate_indicator));
    if ((ac4_substream_info_obj.bitrate_indicator & 0x1) == 1) {
      int more_bits = 0;
      RCHECK(reader->ReadBits(2, &more_bits));
      ac4_substream_info_obj.bitrate_indicator =
          (ac4_substream_info_obj.bitrate_indicator << 2) + more_bits;
    }
  }
  for (int i = 0; i < frame_rate_factor; i++) {
    RCHECK(reader->ReadBits(1, &ac4_substream_info_obj.b_audio_ndot));
  }
  if (b_substreams_present == 1) {
    RCHECK(reader->ReadBits(2, &ac4_substream_info_obj.substream_index));
    if (ac4_substream_info_obj.substream_index == 3) {
      ac4_substream_info_obj.substream_index += ReadAc4VariableBits(reader, 2);
    }
  }
  // sus_ver = 1;
  return true;
}

int AC4Parser::ParseChannelMode(BitReader* reader, int presentation_version) {
  int channel_mode_code = 0;
  int read_more = 0;
  RCHECK(reader->ReadBits(1, &channel_mode_code));
  if (channel_mode_code == 0) {
    return CH_MODE_MONO;
  }
  RCHECK(reader->ReadBits(1, &read_more));
  channel_mode_code = (channel_mode_code << 1) | read_more;
  if (channel_mode_code == 2) {  // Stereo  0b10
    return CH_MODE_STEREO;
  }
  RCHECK(reader->ReadBits(2, &read_more));
  channel_mode_code = (channel_mode_code << 2) | read_more;
  switch (channel_mode_code) {
    case 12:  // 3.0 0b1100
      return CH_MODE_3_0;
    case 13:  // 5.0 0b1101
      return CH_MODE_5_0;
    case 14:  // 5.1 0b1110
      return CH_MODE_5_1;
  }
  RCHECK(reader->ReadBits(3, &read_more));
  channel_mode_code = (channel_mode_code << 3) | read_more;
  switch (channel_mode_code) {
    case 120:                          // 0b1111000
      if (presentation_version == 2) {  // IMS (all content)
        return CH_MODE_STEREO;
      } else {  // 7.0: 3/4/0
        return CH_MODE_70_34;
      }
    case 121:                          // 0b1111001
      if (presentation_version == 2) {  // IMS (Atmos content)
        //dolby_atmos_indicator |= 1;
        return CH_MODE_STEREO;
      } else {  // 7.1: 3/4/0.1
        return CH_MODE_71_34;
      }
    case 122:  // 7.0: 5/2/0   0b1111010
      return CH_MODE_70_52;
    case 123:  // 7.1: 5/2/0.1 0b1111011
      return CH_MODE_71_52;
    case 124:  // 7.0: 3/2/2   0b1111100
      return CH_MODE_70_322;
    case 125:  // 7.1: 3/2/2.1 0b1111101
      return CH_MODE_71_322;
  }
  RCHECK(reader->ReadBits(1, &read_more));
  channel_mode_code = (channel_mode_code << 1) | read_more;
  switch (channel_mode_code) {
    case 252:  // 7.0.4 0b11111100
      return CH_MODE_7_0_4;
    case 253:  // 7.1.4 0b11111101
      return CH_MODE_7_1_4;
  }
  RCHECK(reader->ReadBits(1, &read_more));
  channel_mode_code = (channel_mode_code << 1) | read_more;
  switch (channel_mode_code) {
    case 508:  // 9.0.4 0b111111100
      return CH_MODE_9_0_4;
    case 509:  // 9.1.4 0b111111101
      return CH_MODE_9_1_4;
    case 510:  // 22.2 0b111111110
      return CH_MODE_22_2;
    case 511:  // Reserved, escape value 0b111111111
    default:
      ReadAc4VariableBits(reader, 2);
      return CH_MODE_RESERVED;
  }
}

}  // namespace media
}  // namespace shaka
