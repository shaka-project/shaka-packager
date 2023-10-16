// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/av1_parser.h>

#include <algorithm>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/base/rcheck.h>

namespace shaka {
namespace media {
namespace {

// 3. Symbols and abbreviated terms.
enum MotionType {
  IDENTITY = 0,
  TRANSLATION,
  ROTZOOM,
  AFFINE,
};

const int kSelectScreenContentTools = 2;
const int kSelectIntegerMv = 2;
const int kPrimaryRefNone = 7;
const int kNumRefFrames = 8;
const int kAllFrames = (1 << kNumRefFrames) - 1;

// 6.2.2. OBU header semantics.
enum ObuType {
  OBU_SEQUENCE_HEADER = 1,
  OBU_TEMPORAL_DELIMITER,
  OBU_FRAME_HEADER,
  OBU_TILE_GROUP,
  OBU_METADATA,
  OBU_FRAME,
  OBU_REDUNDENT_FRAME_HEADER,
  OBU_TILE_LIST,
  // Reserved types between OBU_TILE_LIST and OBU_PADDING.
  OBU_PADDING = 15,
};

// 6.4.2. Color config semantics.
enum ColorPrimaries {
  CP_BT_709 = 1,
  CP_UNSPECIFIED = 2,
  // We are not interested in the others.
};
enum TransferCharacteristics {
  TC_UNSPECIFIED = 2,
  TC_SRGB = 13,
  // We are not interested in the others.
};
enum MatrixCoefficients {
  MC_IDENTITY = 0,
  MC_UNSPECIFIED = 2,
  // We are not interested in the others.
};
enum ChromaSamplePosition {
  CSP_UNKNOWN = 0,
  CSP_VERTICAL,
  CSP_COLOCATED,
  CSP_RESERVED,
};

// 6.8.2. Uncompressed header semantics.
enum FrameType {
  KEY_FRAME = 0,
  INTER_FRAME,
  INTRA_ONLY_FRAME,
  SWITCH_FRAME,
};

// 6.10.24. Ref frames semantics.
enum RefFrameName {
  INTRA_FRAME = 0,
  LAST_FRAME,
  LAST2_FRAME,
  LAST3_FRAME,
  GOLDEN_FRAME,
  BWDREF_FRAME,
  ALTREF2_FRAME,
  ALTREF_FRAME,
};

// 4.7. Mathematical functions.
int Clip3(int min_value, int max_value, int value) {
  if (value < min_value)
    return min_value;
  if (value > max_value)
    return max_value;
  return value;
}

// 4.7. Mathematical functions. The FloorLog2(x) function is defined to be the
// floor of the base 2 logarithm of the input x.
int FloorLog2(int x) {
  int s = 0;
  while (x != 0) {
    x = x >> 1;
    s++;
  }
  return s - 1;
}

// 4.10.3. uvlc(). This is a modified form of Exponential-Golomb coding.
bool ReadUvlc(BitReader* reader, uint32_t* val) {
  // Count the number of contiguous zero bits.
  int leading_zeros = 0;
  while (true) {
    bool done = false;
    RCHECK(reader->ReadBits(1, &done));
    if (done)
      break;
    leading_zeros++;
  }

  if (leading_zeros >= 32) {
    *val = (1ull << 32) - 1;
    return true;
  }

  int value = 0;
  if (leading_zeros > 0)
    RCHECK(reader->ReadBits(leading_zeros, &value));

  *val = value + (1 << leading_zeros) - 1;
  return true;
}

// 4.10.4. le(n). Unsigned little-endian n-byte number appearing directly in the
// bitstream.
bool ReadLe(int n, BitReader* reader, size_t* val) {
  size_t t = 0;
  for (int i = 0; i < n; i++) {
    size_t byte = 0;
    RCHECK(reader->ReadBits(8, &byte));
    t += (byte << (i * 8));
  }
  *val = t;
  return true;
}

// 4.10.5. leb128(). Unsigned integer represented by a variable number of
// little-endian bytes.
bool ReadLeb128(BitReader* reader, size_t* size) {
  size_t value = 0;
  for (int i = 0; i < 8; i++) {
    size_t leb128_byte = 0;
    RCHECK(reader->ReadBits(8, &leb128_byte));
    value |= (leb128_byte & 0x7f) << (i * 7);
    if (!(leb128_byte & 0x80))
      break;
  }
  // It is a requirement of bitstream conformance that the value returned from
  // the leb128 parsing process is less than or equal to (1<<32) - 1.
  RCHECK(value <= ((1ull << 32) - 1));
  *size = value;
  return true;
}

// 4.10.6. su(n). Signed integer converted from an n bits unsigned integer in
// the bitstream.
bool ReadSu(int n, BitReader* reader, int* value) {
  RCHECK(reader->ReadBits(n, value));
  int sign_mask = 1 << (n - 1);
  if (*value & sign_mask)
    *value = *value - 2 * sign_mask;
  return true;
}

// 4.10.7. ns(n). Unsigned encoded integer with maximum number of values in n
// (i.e. output in range 0..n-1).
bool ReadNs(int n, BitReader* reader, int* value) {
  const int w = FloorLog2(n) + 1;
  const int m = (1 << w) - n;
  RCHECK(reader->ReadBits(w - 1, value));
  if (*value < m)
    return true;
  int extra_bit = 0;
  RCHECK(reader->ReadBits(1, &extra_bit));
  *value = (*value << 1) - m + extra_bit;
  return true;
}

// 5.9.16. Tile size calculation function: returns the smallest value for k such
// that blk_size << k is greater than or equal to target.
int TileLog2(int blk_size, int target) {
  int k = 0;
  for (k = 0; (blk_size << k) < target; k++)
    continue;
  return k;
}

// See 7.8. Set frame refs process.
int FindLatestBackward(int shifted_order_hints[],
                       bool used_frame[],
                       int cur_frame_hint) {
  int ref = -1;
  int latest_order_hint = 0;
  for (int i = 0; i < kNumRefFrames; i++) {
    const int hint = shifted_order_hints[i];
    if (!used_frame[i] && hint >= cur_frame_hint &&
        (ref < 0 || hint >= latest_order_hint)) {
      ref = i;
      latest_order_hint = hint;
    }
  }
  return ref;
}

// See 7.8. Set frame refs process.
int FindEarliestBackward(int shifted_order_hints[],
                         bool used_frame[],
                         int cur_frame_hint) {
  int ref = -1;
  int earliest_order_hint = 0;
  for (int i = 0; i < kNumRefFrames; i++) {
    const int hint = shifted_order_hints[i];
    if (!used_frame[i] && hint >= cur_frame_hint &&
        (ref < 0 || hint < earliest_order_hint)) {
      ref = i;
      earliest_order_hint = hint;
    }
  }
  return ref;
}

// See 7.8. Set frame refs process.
int FindLatestForward(int shifted_order_hints[],
                      bool used_frame[],
                      int cur_frame_hint) {
  int ref = -1;
  int latest_order_hint = 0;
  for (int i = 0; i < kNumRefFrames; i++) {
    const int hint = shifted_order_hints[i];
    if (!used_frame[i] && hint < cur_frame_hint &&
        (ref < 0 || hint >= latest_order_hint)) {
      ref = i;
      latest_order_hint = hint;
    }
  }
  return ref;
}

}  // namespace

AV1Parser::AV1Parser() = default;
AV1Parser::~AV1Parser() = default;

bool AV1Parser::Parse(const uint8_t* data,
                      size_t data_size,
                      std::vector<Tile>* tiles) {
  tiles->clear();

  BitReader reader(data, data_size);
  while (reader.bits_available() > 0) {
    if (!ParseOpenBitstreamUnit(&reader, tiles))
      return false;
  }
  return true;
}

// 5.3.1. General OBU syntax.
bool AV1Parser::ParseOpenBitstreamUnit(BitReader* reader,
                                       std::vector<Tile>* tiles) {
  ObuHeader obu_header;
  RCHECK(ParseObuHeader(reader, &obu_header));

  size_t obu_size = 0;
  if (obu_header.obu_has_size_field)
    RCHECK(ReadLeb128(reader, &obu_size));
  else
    obu_size = reader->bits_available() / 8;

  VLOG(4) << "OBU " << obu_header.obu_type << " size " << obu_size;

  const size_t start_position = reader->bit_position();
  switch (obu_header.obu_type) {
    case OBU_SEQUENCE_HEADER:
      RCHECK(ParseSequenceHeaderObu(reader));
      break;
    case OBU_FRAME_HEADER:
    case OBU_REDUNDENT_FRAME_HEADER:
      RCHECK(ParseFrameHeaderObu(obu_header, reader));
      break;
    case OBU_TILE_GROUP:
      RCHECK(ParseTileGroupObu(obu_size, reader, tiles));
      break;
    case OBU_FRAME:
      RCHECK(ParseFrameObu(obu_header, obu_size, reader, tiles));
      break;
    default:
      // Skip all OBUs we are not interested.
      RCHECK(reader->SkipBits(obu_size * 8));
      break;
  }

  const size_t current_position = reader->bit_position();
  const size_t payload_bits = current_position - start_position;
  if (obu_header.obu_type == OBU_TILE_GROUP ||
      obu_header.obu_type == OBU_FRAME) {
    RCHECK(payload_bits == obu_size * 8);
  } else if (obu_size > 0) {
    RCHECK(payload_bits <= obu_size * 8);
    RCHECK(ParseTrailingBits(obu_size * 8 - payload_bits, reader));
  }
  return true;
}

// 5.3.2. OBU header syntax.
bool AV1Parser::ParseObuHeader(BitReader* reader, ObuHeader* obu_header) {
  int obu_forbidden_bit = 0;
  RCHECK(reader->ReadBits(1, &obu_forbidden_bit));
  RCHECK(obu_forbidden_bit == 0);
  RCHECK(reader->ReadBits(4, &obu_header->obu_type));
  bool obu_extension_flag = false;
  RCHECK(reader->ReadBits(1, &obu_extension_flag));
  RCHECK(reader->ReadBits(1, &obu_header->obu_has_size_field));
  RCHECK(reader->SkipBits(1));  // Skip obu_reserved_1bit.

  if (obu_extension_flag)
    RCHECK(ParseObuExtensionHeader(reader, &obu_header->extension_header));

  return true;
}

// 5.3.3. OBU extension header syntax.
bool AV1Parser::ParseObuExtensionHeader(
    BitReader* reader,
    ObuExtensionHeader* obu_extension_header) {
  RCHECK(reader->ReadBits(3, &obu_extension_header->temporal_id));
  RCHECK(reader->ReadBits(2, &obu_extension_header->spatial_id));
  RCHECK(reader->SkipBits(3));  // Skip extension_header_reserved_3bits.
  return true;
}

// 5.3.4. Trailing bits syntax.
bool AV1Parser::ParseTrailingBits(size_t nb_bits, BitReader* reader) {
  int trailing_one_bit = 0;
  RCHECK(reader->ReadBits(1, &trailing_one_bit));
  RCHECK(trailing_one_bit == 1);
  nb_bits--;
  while (nb_bits > 0) {
    int trailing_zero_bit = 0;
    RCHECK(reader->ReadBits(1, &trailing_zero_bit));
    RCHECK(trailing_zero_bit == 0);
    nb_bits--;
  }
  return true;
}

bool AV1Parser::ByteAlignment(BitReader* reader) {
  while (reader->bit_position() & 7) {
    int zero_bit = 0;
    RCHECK(reader->ReadBits(1, &zero_bit));
    RCHECK(zero_bit == 0);
  }
  return true;
}

// 5.5.1. General sequence header OBU syntax.
bool AV1Parser::ParseSequenceHeaderObu(BitReader* reader) {
  RCHECK(reader->ReadBits(3, &sequence_header_.seq_profile));
  // Skip still_picture.
  RCHECK(reader->SkipBits(1));

  RCHECK(reader->ReadBits(1, &sequence_header_.reduced_still_picture_header));
  if (sequence_header_.reduced_still_picture_header) {
    sequence_header_.decoder_model_info_present_flag = false;
    sequence_header_.operating_points_cnt_minus_1 = 0;
    sequence_header_.operating_point_idc[0] = 0;
    // Skip seq_level_idx[0].
    RCHECK(reader->SkipBits(5));
    sequence_header_.decoder_model_present_for_this_op[0] = false;
  } else {
    bool timing_info_present_flag = false;
    RCHECK(reader->ReadBits(1, &timing_info_present_flag));

    bool decoder_model_info_present_flag = false;
    if (timing_info_present_flag) {
      RCHECK(ParseTimingInfo(reader));
      RCHECK(reader->ReadBits(1, &decoder_model_info_present_flag));
      if (decoder_model_info_present_flag)
        RCHECK(ParseDecoderModelInfo(reader));
    }
    sequence_header_.decoder_model_info_present_flag =
        decoder_model_info_present_flag;

    bool initial_display_delay_present_flag = false;
    RCHECK(reader->ReadBits(1, &initial_display_delay_present_flag));

    RCHECK(reader->ReadBits(5, &sequence_header_.operating_points_cnt_minus_1));
    for (int i = 0; i <= sequence_header_.operating_points_cnt_minus_1; i++) {
      RCHECK(reader->ReadBits(12, &sequence_header_.operating_point_idc[i]));
      int seq_level_idx_i = 0;
      RCHECK(reader->ReadBits(5, &seq_level_idx_i));
      if (seq_level_idx_i > 7) {
        // Skip seq_tier[i].
        RCHECK(reader->SkipBits(1));
      }

      if (sequence_header_.decoder_model_info_present_flag) {
        RCHECK(reader->ReadBits(
            1, &sequence_header_.decoder_model_present_for_this_op[i]));
        if (sequence_header_.decoder_model_present_for_this_op[i]) {
          RCHECK(SkipOperatingParametersInfo(reader));
        }
      } else {
        sequence_header_.decoder_model_present_for_this_op[i] = false;
      }

      if (initial_display_delay_present_flag) {
        // Skip initial_display_delay_present_for_this_op[i],
        // initial_display_delay_minus_1[i].
        RCHECK(reader->SkipBitsConditional(true, 4));
      }
    }
  }

  RCHECK(reader->ReadBits(4, &sequence_header_.frame_width_bits_minus_1));
  RCHECK(reader->ReadBits(4, &sequence_header_.frame_height_bits_minus_1));
  RCHECK(reader->ReadBits(sequence_header_.frame_width_bits_minus_1 + 1,
                          &sequence_header_.max_frame_width_minus_1));
  RCHECK(reader->ReadBits(sequence_header_.frame_height_bits_minus_1 + 1,
                          &sequence_header_.max_frame_height_minus_1));

  if (sequence_header_.reduced_still_picture_header) {
    sequence_header_.frame_id_numbers_present_flag = false;
  } else {
    RCHECK(
        reader->ReadBits(1, &sequence_header_.frame_id_numbers_present_flag));
  }
  if (sequence_header_.frame_id_numbers_present_flag) {
    RCHECK(
        reader->ReadBits(4, &sequence_header_.delta_frame_id_length_minus_2));
    RCHECK(reader->ReadBits(
        3, &sequence_header_.additional_frame_id_length_minus_1));
  }

  RCHECK(reader->ReadBits(1, &sequence_header_.use_128x128_superblock));
  // Skip enable_filter_intra, enable_intra_edge_filter.
  RCHECK(reader->SkipBits(1 + 1));

  if (sequence_header_.reduced_still_picture_header) {
    sequence_header_.enable_warped_motion = false;
    sequence_header_.enable_order_hint = false;
    sequence_header_.enable_ref_frame_mvs = false;
    sequence_header_.order_hint_bits = 0;
    sequence_header_.seq_force_screen_content_tools = kSelectScreenContentTools;
    sequence_header_.seq_force_integer_mv = kSelectIntegerMv;
  } else {
    // Skip enable_interintra_compound, enable_masked_compound,
    RCHECK(reader->SkipBits(1 + 1));

    RCHECK(reader->ReadBits(1, &sequence_header_.enable_warped_motion));
    RCHECK(reader->SkipBits(1));  // Skip enable_dual_filter.
    RCHECK(reader->ReadBits(1, &sequence_header_.enable_order_hint));
    if (sequence_header_.enable_order_hint) {
      // Skip enable_jnt_comp.
      RCHECK(reader->SkipBits(1));
      RCHECK(reader->ReadBits(1, &sequence_header_.enable_ref_frame_mvs));
    } else {
      sequence_header_.enable_ref_frame_mvs = false;
    }

    bool seq_choose_screen_content_tools = false;
    RCHECK(reader->ReadBits(1, &seq_choose_screen_content_tools));

    if (seq_choose_screen_content_tools) {
      sequence_header_.seq_force_screen_content_tools =
          kSelectScreenContentTools;
    } else {
      RCHECK(reader->ReadBits(
          1, &sequence_header_.seq_force_screen_content_tools));
    }

    if (sequence_header_.seq_force_screen_content_tools > 0) {
      bool seq_choose_integer_mv = false;
      RCHECK(reader->ReadBits(1, &seq_choose_integer_mv));
      if (seq_choose_integer_mv)
        sequence_header_.seq_force_integer_mv = kSelectIntegerMv;
      else
        RCHECK(reader->ReadBits(1, &sequence_header_.seq_force_integer_mv));
    } else {
      sequence_header_.seq_force_integer_mv = kSelectIntegerMv;
    }

    if (sequence_header_.enable_order_hint) {
      int order_hint_bits_minus_1 = 0;
      RCHECK(reader->ReadBits(3, &order_hint_bits_minus_1));
      sequence_header_.order_hint_bits = order_hint_bits_minus_1 + 1;
    } else {
      sequence_header_.order_hint_bits = 0;
    }
  }

  RCHECK(reader->ReadBits(1, &sequence_header_.enable_superres));
  RCHECK(reader->ReadBits(1, &sequence_header_.enable_cdef));
  RCHECK(reader->ReadBits(1, &sequence_header_.enable_restoration));
  RCHECK(ParseColorConfig(reader));
  RCHECK(reader->ReadBits(1, &sequence_header_.film_grain_params_present));
  return true;
}

// 5.5.2. Color config syntax.
bool AV1Parser::ParseColorConfig(BitReader* reader) {
  ColorConfig& color_config = sequence_header_.color_config;

  bool high_bitdepth = false;
  RCHECK(reader->ReadBits(1, &high_bitdepth));
  if (sequence_header_.seq_profile == 2 && high_bitdepth) {
    bool twelve_bit = false;
    RCHECK(reader->ReadBits(1, &twelve_bit));
    color_config.bit_depth = twelve_bit ? 12 : 10;
  } else if (sequence_header_.seq_profile <= 2) {
    color_config.bit_depth = high_bitdepth ? 10 : 8;
  }

  if (sequence_header_.seq_profile == 1)
    color_config.mono_chrome = 0;
  else
    RCHECK(reader->ReadBits(1, &color_config.mono_chrome));
  color_config.num_planes = color_config.mono_chrome ? 1 : 3;

  bool color_description_present_flag = false;
  RCHECK(reader->ReadBits(1, &color_description_present_flag));

  if (color_description_present_flag) {
    RCHECK(reader->ReadBits(8, &color_config.color_primaries));
    RCHECK(reader->ReadBits(8, &color_config.transfer_chracteristics));
    RCHECK(reader->ReadBits(8, &color_config.matrix_coefficients));
  } else {
    color_config.color_primaries = CP_UNSPECIFIED;
    color_config.transfer_chracteristics = TC_UNSPECIFIED;
    color_config.matrix_coefficients = MC_UNSPECIFIED;
  }

  if (color_config.mono_chrome) {
    RCHECK(reader->ReadBits(1, &color_config.color_range));
    color_config.subsampling_x = true;
    color_config.subsampling_y = true;
    color_config.chroma_sampling_position = CSP_UNKNOWN;
    color_config.separate_uv_delta_q = false;
    return true;
  } else if (color_config.color_primaries == CP_BT_709 &&
             color_config.transfer_chracteristics == TC_SRGB &&
             color_config.matrix_coefficients == MC_IDENTITY) {
    color_config.color_range = true;
    color_config.subsampling_x = false;
    color_config.subsampling_y = false;
  } else {
    RCHECK(reader->ReadBits(1, &color_config.color_range));
    if (sequence_header_.seq_profile == 0) {
      color_config.subsampling_x = true;
      color_config.subsampling_y = true;
    } else if (sequence_header_.seq_profile == 1) {
      color_config.subsampling_x = false;
      color_config.subsampling_y = false;
    } else {
      if (color_config.bit_depth == 12) {
        RCHECK(reader->ReadBits(1, &color_config.subsampling_x));
        if (color_config.subsampling_x)
          RCHECK(reader->ReadBits(1, &color_config.subsampling_y));
        else
          color_config.subsampling_y = false;
      } else {
        color_config.subsampling_x = true;
        color_config.subsampling_y = false;
      }
    }

    if (color_config.subsampling_x && color_config.subsampling_y)
      RCHECK(reader->ReadBits(2, &color_config.chroma_sampling_position));
  }

  RCHECK(reader->ReadBits(1, &color_config.separate_uv_delta_q));
  return true;
}

// 5.5.3.Timing info syntax.
bool AV1Parser::ParseTimingInfo(BitReader* reader) {
  // Skip num_units_in_display_tick, time_scale.
  RCHECK(reader->SkipBits(32 + 32));
  bool equal_picture_interval = false;
  RCHECK(reader->ReadBits(1, &equal_picture_interval));
  sequence_header_.timing_info.equal_picture_interval = equal_picture_interval;
  if (equal_picture_interval) {
    uint32_t num_ticks_per_picture_minus_1 = 0;
    RCHECK(ReadUvlc(reader, &num_ticks_per_picture_minus_1));
  }
  return true;
}

// 5.5.4. Decoder model info syntax.
bool AV1Parser::ParseDecoderModelInfo(BitReader* reader) {
  DecoderModelInfo& decoder_model_info = sequence_header_.decoder_model_info;

  RCHECK(reader->ReadBits(5, &decoder_model_info.buffer_delay_length_minus_1));
  // Skip num_units_in_decoding_tick.
  RCHECK(reader->SkipBits(32));
  RCHECK(reader->ReadBits(
      5, &decoder_model_info.buffer_removal_time_length_minus_1));
  RCHECK(reader->ReadBits(
      5, &decoder_model_info.frame_presentation_time_length_minus_1));
  return true;
}

// 5.5.5. Operating parameters info syntax.
bool AV1Parser::SkipOperatingParametersInfo(BitReader* reader) {
  const int n =
      sequence_header_.decoder_model_info.buffer_delay_length_minus_1 + 1;
  // Skip decoder_buffer_delay[op], encoder_buffer_delay[op],
  // low_delay_mode_flag[op].
  RCHECK(reader->SkipBits(n + n + 1));
  return true;
}

// 5.9.1. General frame header OBU syntax.
bool AV1Parser::ParseFrameHeaderObu(const ObuHeader& obu_header,
                                    BitReader* reader) {
  if (frame_header_.seen_frame_header)
    return true;

  frame_header_.seen_frame_header = true;
  RCHECK(ParseUncompressedHeader(obu_header, reader));
  if (frame_header_.show_existing_frame) {
    DecodeFrameWrapup();
    frame_header_.seen_frame_header = false;
  } else {
    frame_header_.seen_frame_header = true;
  }
  return true;
}

// 5.9.2. Uncompressed header syntax.
bool AV1Parser::ParseUncompressedHeader(const ObuHeader& obu_header,
                                        BitReader* reader) {
  int id_len = 0;
  if (sequence_header_.frame_id_numbers_present_flag) {
    id_len = sequence_header_.additional_frame_id_length_minus_1 + 1 +
             sequence_header_.delta_frame_id_length_minus_2 + 2;
  }

  bool frame_is_intra = false;
  bool show_frame = false;
  bool showable_frame = false;
  bool error_resilient_mode = false;

  if (sequence_header_.reduced_still_picture_header) {
    frame_header_.show_existing_frame = false;
    frame_header_.frame_type = KEY_FRAME;
    frame_is_intra = true;
    show_frame = true;
    showable_frame = false;
  } else {
    RCHECK(reader->ReadBits(1, &frame_header_.show_existing_frame));
    if (frame_header_.show_existing_frame) {
      RCHECK(reader->ReadBits(3, &frame_header_.frame_to_show_map_idx));
      if (sequence_header_.decoder_model_info_present_flag &&
          !sequence_header_.timing_info.equal_picture_interval) {
        RCHECK(SkipTemporalPointInfo(reader));
      }
      frame_header_.refresh_frame_flags = 0;
      if (sequence_header_.frame_id_numbers_present_flag) {
        // Skip display_frame_id.
        RCHECK(reader->SkipBits(id_len));
      }
      frame_header_.frame_type =
          reference_frames_[frame_header_.frame_to_show_map_idx].frame_type;
      if (frame_header_.frame_type == KEY_FRAME) {
        frame_header_.refresh_frame_flags = kAllFrames;
      }
      return true;
    }

    RCHECK(reader->ReadBits(2, &frame_header_.frame_type));
    frame_is_intra = frame_header_.frame_type == INTRA_ONLY_FRAME ||
                     frame_header_.frame_type == KEY_FRAME;
    RCHECK(reader->ReadBits(1, &show_frame));
    if (show_frame && sequence_header_.decoder_model_info_present_flag &&
        !sequence_header_.timing_info.equal_picture_interval) {
      RCHECK(SkipTemporalPointInfo(reader));
    }
    if (show_frame)
      showable_frame = frame_header_.frame_type != KEY_FRAME;
    else
      RCHECK(reader->ReadBits(1, &showable_frame));

    if (frame_header_.frame_type == SWITCH_FRAME ||
        (frame_header_.frame_type == KEY_FRAME && show_frame)) {
      error_resilient_mode = true;
    } else {
      RCHECK(reader->ReadBits(1, &error_resilient_mode));
    }
  }

  if (frame_header_.frame_type == KEY_FRAME && show_frame) {
    for (int i = 0; i < kNumRefFrames; i++) {
      reference_frames_[i].order_hint = 0;
    }
  }

  bool disable_cdf_update = false;
  RCHECK(reader->ReadBits(1, &disable_cdf_update));

  bool allow_screen_content_tools = false;
  if (sequence_header_.seq_force_screen_content_tools ==
      kSelectScreenContentTools) {
    RCHECK(reader->ReadBits(1, &allow_screen_content_tools));
  } else {
    allow_screen_content_tools =
        sequence_header_.seq_force_screen_content_tools != 0;
  }

  int force_integer_mv = 0;
  if (allow_screen_content_tools) {
    if (sequence_header_.seq_force_integer_mv == kSelectIntegerMv)
      RCHECK(reader->ReadBits(1, &force_integer_mv));
    else
      force_integer_mv = sequence_header_.seq_force_integer_mv;
  }
  if (frame_is_intra)
    force_integer_mv = 1;

  if (sequence_header_.frame_id_numbers_present_flag) {
    // Skip current_frame_id.
    RCHECK(reader->SkipBits(id_len));
  }

  bool frame_size_override_flag = false;
  if (frame_header_.frame_type == SWITCH_FRAME)
    frame_size_override_flag = true;
  else if (sequence_header_.reduced_still_picture_header)
    frame_size_override_flag = false;
  else
    RCHECK(reader->ReadBits(1, &frame_size_override_flag));

  RCHECK(reader->ReadBits(sequence_header_.order_hint_bits,
                          &frame_header_.order_hint));
  int primary_ref_frame = 0;
  if (frame_is_intra || error_resilient_mode) {
    primary_ref_frame = kPrimaryRefNone;
  } else {
    RCHECK(reader->ReadBits(3, &primary_ref_frame));
  }
  if (sequence_header_.decoder_model_info_present_flag) {
    bool buffer_removal_time_present_flag = false;
    RCHECK(reader->ReadBits(1, &buffer_removal_time_present_flag));
    if (buffer_removal_time_present_flag) {
      for (int op_num = 0;
           op_num <= sequence_header_.operating_points_cnt_minus_1; op_num++) {
        if (sequence_header_.decoder_model_present_for_this_op[op_num]) {
          const int op_pt_idc = sequence_header_.operating_point_idc[op_num];
          const int in_temporal_layer =
              (op_pt_idc >> obu_header.extension_header.temporal_id) & 1;
          const int in_spatial_layer =
              (op_pt_idc >> (obu_header.extension_header.spatial_id + 8)) & 1;
          if (op_pt_idc == 0 || (in_temporal_layer && in_spatial_layer)) {
            // Skip buffer_removal_time[ opNum ].
            RCHECK(reader->SkipBits(sequence_header_.decoder_model_info
                                        .buffer_removal_time_length_minus_1 +
                                    1));
          }
        }
      }
    }
  }

  bool allow_high_precision_mv = false;
  bool allow_intrabc = false;

  if (frame_header_.frame_type == SWITCH_FRAME ||
      (frame_header_.frame_type == KEY_FRAME && show_frame)) {
    frame_header_.refresh_frame_flags = kAllFrames;
  } else {
    RCHECK(reader->ReadBits(8, &frame_header_.refresh_frame_flags));
  }
  if (!frame_is_intra || frame_header_.refresh_frame_flags != kAllFrames) {
    if (error_resilient_mode && sequence_header_.enable_order_hint) {
      for (int i = 0; i < kNumRefFrames; i++) {
        // Skip ref_order_hint[ i ].
        RCHECK(reader->SkipBits(sequence_header_.order_hint_bits));
      }
    }
  }

  if (frame_is_intra) {
    RCHECK(ParseFrameSize(frame_size_override_flag, reader));
    RCHECK(ParseRenderSize(reader));
    if (allow_screen_content_tools &&
        frame_header_.upscaled_width == frame_header_.frame_width)
      RCHECK(reader->ReadBits(1, &allow_intrabc));
  } else {
    bool frame_refs_short_signaling = false;
    if (sequence_header_.enable_order_hint) {
      RCHECK(reader->ReadBits(1, &frame_refs_short_signaling));
      if (frame_refs_short_signaling) {
        int last_frame_idx = 0;
        RCHECK(reader->ReadBits(3, &last_frame_idx));
        int gold_frame_idx = 0;
        RCHECK(reader->ReadBits(3, &gold_frame_idx));
        RCHECK(SetFrameRefs(last_frame_idx, gold_frame_idx));
      }
    }
    for (int i = 0; i < kRefsPerFrame; i++) {
      if (!frame_refs_short_signaling)
        RCHECK(reader->ReadBits(3, &frame_header_.ref_frame_idx[i]));
      if (sequence_header_.frame_id_numbers_present_flag) {
        // Skip delta_frame_id_minus_1.
        RCHECK(reader->SkipBits(sequence_header_.delta_frame_id_length_minus_2 +
                                2));
      }
    }
    if (frame_size_override_flag && !error_resilient_mode) {
      RCHECK(ParseFrameSizeWithRefs(frame_size_override_flag, reader));
    } else {
      RCHECK(ParseFrameSize(frame_size_override_flag, reader));
      RCHECK(ParseRenderSize(reader));
    }

    if (force_integer_mv)
      allow_high_precision_mv = false;
    else
      RCHECK(reader->ReadBits(1, &allow_high_precision_mv));

    RCHECK(SkipInterpolationFilter(reader));
    // Skip is_motion_mode_switchable.
    RCHECK(reader->SkipBits(1));
    if (!error_resilient_mode && sequence_header_.enable_ref_frame_mvs) {
      // Skip use_ref_frame_mvs.
      RCHECK(reader->SkipBits(1));
    }
  }

  if (!sequence_header_.reduced_still_picture_header && !disable_cdf_update) {
    // Skip disable_frame_end_update_cdf.
    RCHECK(reader->SkipBits(1));
  }

  RCHECK(ParseTileInfo(reader));
  RCHECK(ParseQuantizationParams(reader));
  RCHECK(ParseSegmentationParams(primary_ref_frame, reader));

  bool delta_q_present = false;
  RCHECK(SkipDeltaQParams(reader, &delta_q_present));
  RCHECK(SkipDeltaLfParams(delta_q_present, allow_intrabc, reader));

  const auto& quantization_params = frame_header_.quantization_params;
  bool coded_lossless = true;
  for (int segment_id = 0; segment_id < kMaxSegments; segment_id++) {
    const int qindex = GetQIndex(true, segment_id);
    const bool lossless = qindex == 0 && quantization_params.delta_qydc == 0 &&
                          quantization_params.delta_quac == 0 &&
                          quantization_params.delta_qudc == 0 &&
                          quantization_params.delta_qvac == 0 &&
                          quantization_params.delta_qvdc == 0;
    if (!lossless)
      coded_lossless = false;
  }
  const bool all_lossless = coded_lossless && (frame_header_.frame_width ==
                                               frame_header_.upscaled_width);

  RCHECK(ParseLoopFilterParams(coded_lossless, allow_intrabc, reader));
  RCHECK(ParseCdefParams(coded_lossless, allow_intrabc, reader));
  RCHECK(ParseLrParams(all_lossless, allow_intrabc, reader));
  RCHECK(SkipTxMode(coded_lossless, reader));
  bool reference_select = false;
  RCHECK(ParseFrameReferenceMode(frame_is_intra, reader, &reference_select));
  RCHECK(SkipSkipModeParams(frame_is_intra, reference_select, reader));

  bool allow_warped_motion = false;
  if (frame_is_intra || error_resilient_mode ||
      !sequence_header_.enable_warped_motion) {
    allow_warped_motion = false;
  } else {
    RCHECK(reader->ReadBits(1, &allow_warped_motion));
  }
  // Skip reduced_tx_set.
  RCHECK(reader->SkipBits(1));

  RCHECK(
      SkipGlobalMotionParams(frame_is_intra, allow_high_precision_mv, reader));
  RCHECK(SkipFilmGrainParams(show_frame, showable_frame, reader));
  return true;
}

// 5.9.3. Get relative distance function.
int AV1Parser::GetRelativeDist(int a, int b) {
  if (!sequence_header_.enable_order_hint)
    return 0;
  int diff = a - b;
  const int m = 1 << (sequence_header_.order_hint_bits - 1);
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

// 5.9.5. Frame size syntax.
bool AV1Parser::ParseFrameSize(bool frame_size_override_flag,
                               BitReader* reader) {
  if (frame_size_override_flag) {
    int frame_width_minus_1 = 0;
    RCHECK(reader->ReadBits(sequence_header_.frame_width_bits_minus_1 + 1,
                            &frame_width_minus_1));
    int frame_height_minus_1 = 0;
    RCHECK(reader->ReadBits(sequence_header_.frame_height_bits_minus_1 + 1,
                            &frame_height_minus_1));
    frame_header_.frame_width = frame_width_minus_1 + 1;
    frame_header_.frame_height = frame_height_minus_1 + 1;
  } else {
    frame_header_.frame_width = sequence_header_.max_frame_width_minus_1 + 1;
    frame_header_.frame_height = sequence_header_.max_frame_height_minus_1 + 1;
  }
  RCHECK(ParseSuperresParams(reader));
  ComputeImageSize();
  return true;
}

// 5.9.6. Render size syntax.
bool AV1Parser::ParseRenderSize(BitReader* reader) {
  bool render_and_frame_size_different = false;
  RCHECK(reader->ReadBits(1, &render_and_frame_size_different));
  if (render_and_frame_size_different) {
    int render_width_minus_1 = 0;
    RCHECK(reader->ReadBits(16, &render_width_minus_1));
    int render_height_minus_1 = 0;
    RCHECK(reader->ReadBits(16, &render_height_minus_1));
    frame_header_.render_width = render_width_minus_1 + 1;
    frame_header_.render_height = render_height_minus_1 + 1;
  } else {
    frame_header_.render_width = frame_header_.upscaled_width;
    frame_header_.render_height = frame_header_.frame_height;
  }
  return true;
}

// 5.9.7. Frame size with refs syntax.
bool AV1Parser::ParseFrameSizeWithRefs(bool frame_size_override_flag,
                                       BitReader* reader) {
  bool found_ref = false;
  for (int i = 0; i < kRefsPerFrame; i++) {
    RCHECK(reader->ReadBits(1, &found_ref));
    if (found_ref) {
      const ReferenceFrame& reference_frame =
          reference_frames_[frame_header_.ref_frame_idx[i]];
      frame_header_.upscaled_width = reference_frame.upscaled_width;
      frame_header_.frame_width = frame_header_.upscaled_width;
      frame_header_.frame_height = reference_frame.frame_height;
      frame_header_.render_width = reference_frame.render_width;
      frame_header_.render_height = reference_frame.render_height;
      break;
    }
  }
  if (!found_ref) {
    RCHECK(ParseFrameSize(frame_size_override_flag, reader));
    RCHECK(ParseRenderSize(reader));
  } else {
    RCHECK(ParseSuperresParams(reader));
    ComputeImageSize();
  }
  return true;
}

// 5.9.8. Superres params syntax.
bool AV1Parser::ParseSuperresParams(BitReader* reader) {
  const int kSuperresNum = 8;
  const int kSuperresDenomMin = 9;
  const int kSuperresDenomBits = 3;

  bool use_superres = false;
  if (sequence_header_.enable_superres)
    RCHECK(reader->ReadBits(1, &use_superres));

  int superres_denom = 0;
  if (use_superres) {
    int coded_denom = 0;
    RCHECK(reader->ReadBits(kSuperresDenomBits, &coded_denom));
    superres_denom = coded_denom + kSuperresDenomMin;
  } else {
    superres_denom = kSuperresNum;
  }

  const int upscaled_width = frame_header_.frame_width;
  frame_header_.upscaled_width =
      (upscaled_width * kSuperresNum + superres_denom / 2) / superres_denom;
  return true;
}

// 5.9.9. Compute image size function.
void AV1Parser::ComputeImageSize() {
  frame_header_.mi_cols = 2 * ((frame_header_.frame_width + 7) >> 3);
  frame_header_.mi_rows = 2 * ((frame_header_.frame_height + 7) >> 3);
}

// 5.9.10. Interpolation filter syntax.
bool AV1Parser::SkipInterpolationFilter(BitReader* reader) {
  // SKip is_filter_switchable, interpolation_filter.
  RCHECK(reader->SkipBitsConditional(false, 2));
  return true;
}

// 5.9.11. Loop filter parms syntax.
bool AV1Parser::ParseLoopFilterParams(bool coded_lossless,
                                      bool allow_intrabc,
                                      BitReader* reader) {
  if (coded_lossless || allow_intrabc)
    return true;

  int loop_filter_level[] = {0, 0};
  RCHECK(reader->ReadBits(6, &loop_filter_level[0]));
  RCHECK(reader->ReadBits(6, &loop_filter_level[1]));
  if (sequence_header_.color_config.num_planes > 1) {
    if (loop_filter_level[0] || loop_filter_level[1]) {
      // Skip loop_filter_level[2], loop_filter_level[3].
      RCHECK(reader->SkipBits(6 + 6));
    }
  }
  // Skip loop_filter_sharpness.
  RCHECK(reader->SkipBits(3));
  bool loop_filter_delta_enabled = false;
  RCHECK(reader->ReadBits(1, &loop_filter_delta_enabled));
  if (loop_filter_delta_enabled) {
    bool loop_filter_delta_update = false;
    RCHECK(reader->ReadBits(1, &loop_filter_delta_update));
    if (loop_filter_delta_update) {
      const int kTotalRefsPerFrame = 8;
      for (int i = 0; i < kTotalRefsPerFrame; i++) {
        // Skip update_ref_delta, loop_filter_ref_delta[ i ].
        RCHECK(reader->SkipBitsConditional(true, 1 + 6));
      }
      for (int i = 0; i < 2; i++) {
        // Skip update_mode_delta, loop_filter_mode_delta[ i ].
        RCHECK(reader->SkipBitsConditional(true, 1 + 6));
      }
    }
  }
  return true;
}

// 5.9.12. Quantization params syntax.
bool AV1Parser::ParseQuantizationParams(BitReader* reader) {
  QuantizationParams& quantization_params = frame_header_.quantization_params;

  RCHECK(reader->ReadBits(8, &quantization_params.base_q_idx));
  RCHECK(ReadDeltaQ(reader, &quantization_params.delta_qydc));

  const ColorConfig& color_config = sequence_header_.color_config;
  if (color_config.num_planes > 1) {
    bool diff_uv_delta = false;
    if (color_config.separate_uv_delta_q)
      RCHECK(reader->ReadBits(1, &diff_uv_delta));
    RCHECK(ReadDeltaQ(reader, &quantization_params.delta_qudc));
    RCHECK(ReadDeltaQ(reader, &quantization_params.delta_quac));
    if (diff_uv_delta) {
      RCHECK(ReadDeltaQ(reader, &quantization_params.delta_qvdc));
      RCHECK(ReadDeltaQ(reader, &quantization_params.delta_qvac));
    } else {
      quantization_params.delta_qvdc = quantization_params.delta_qudc;
      quantization_params.delta_qvac = quantization_params.delta_quac;
    }
  } else {
    quantization_params.delta_qudc = 0;
    quantization_params.delta_quac = 0;
    quantization_params.delta_qvdc = 0;
    quantization_params.delta_qvac = 0;
  }
  bool using_qmatrix = false;
  RCHECK(reader->ReadBits(1, &using_qmatrix));
  if (using_qmatrix) {
    // Skip qm_y, qm_u.
    RCHECK(reader->SkipBits(4 + 4));
    if (color_config.separate_uv_delta_q) {
      // Skip qm_v.
      RCHECK(reader->SkipBits(4));
    }
  }
  return true;
}

// 5.9.13. Delta quantizer syntax.
bool AV1Parser::ReadDeltaQ(BitReader* reader, int* delta_q) {
  bool delta_coded = false;
  RCHECK(reader->ReadBits(1, &delta_coded));
  if (delta_coded)
    RCHECK(ReadSu(1 + 6, reader, delta_q));
  else
    *delta_q = 0;
  return true;
}

// 5.9.14. Segmentation params syntax.
bool AV1Parser::ParseSegmentationParams(int primary_ref_frame,
                                        BitReader* reader) {
  SegmentationParams& segmentation_params = frame_header_.segmentation_params;

  RCHECK(reader->ReadBits(1, &segmentation_params.segmentation_enabled));
  if (segmentation_params.segmentation_enabled) {
    bool segmentation_update_data = false;
    if (primary_ref_frame == kPrimaryRefNone) {
      segmentation_update_data = true;
    } else {
      // Skip segmentation_update_map, segmentation_temporal_update.
      RCHECK(reader->SkipBitsConditional(true, 1));
      RCHECK(reader->ReadBits(1, &segmentation_update_data));
    }
    if (segmentation_update_data) {
      static const int kSegmentationFeatureBits[kSegLvlMax] = {8, 6, 6, 6,
                                                               6, 3, 0, 0};
      static const int kSegmentationFeatureSigned[kSegLvlMax] = {1, 1, 1, 1,
                                                                 1, 0, 0, 0};
      const int kMaxLoopFilter = 63;
      static const int kSegmentationFeatureMax[kSegLvlMax] = {255,
                                                              kMaxLoopFilter,
                                                              kMaxLoopFilter,
                                                              kMaxLoopFilter,
                                                              kMaxLoopFilter,
                                                              7,
                                                              0,
                                                              0};

      for (int i = 0; i < kMaxSegments; i++) {
        for (int j = 0; j < kSegLvlMax; j++) {
          bool feature_enabled = false;
          RCHECK(reader->ReadBits(1, &feature_enabled));
          segmentation_params.feature_enabled[i][j] = feature_enabled;
          int clipped_value = 0;
          if (feature_enabled) {
            const int bits_to_read = kSegmentationFeatureBits[j];
            const int limit = kSegmentationFeatureMax[j];
            if (kSegmentationFeatureSigned[j]) {
              int feature_value = 0;
              RCHECK(ReadSu(1 + bits_to_read, reader, &feature_value));
              clipped_value = Clip3(-limit, limit, feature_value);
            } else {
              int feature_value = 0;
              RCHECK(reader->ReadBits(bits_to_read, &feature_value));
              clipped_value = Clip3(0, limit, feature_value);
            }
          }
          segmentation_params.feature_data[i][j] = clipped_value;
        }
      }
    }
  } else {
    for (int i = 0; i < kMaxSegments; i++) {
      for (int j = 0; j < kSegLvlMax; j++) {
        segmentation_params.feature_enabled[i][j] = false;
        segmentation_params.feature_data[i][j] = 0;
      }
    }
  }
  return true;
}

// 5.9.15. Tile info syntax.
bool AV1Parser::ParseTileInfo(BitReader* reader) {
  const int kMaxTileWidth = 4096;
  const int kMaxTileArea = 4096 * 2304;
  const int kMaxTileRows = 64;
  const int kMaxTileCols = 64;

  TileInfo& tile_info = frame_header_.tile_info;

  const int sb_cols = sequence_header_.use_128x128_superblock
                          ? ((frame_header_.mi_cols + 31) >> 5)
                          : ((frame_header_.mi_cols + 15) >> 4);
  const int sb_rows = sequence_header_.use_128x128_superblock
                          ? ((frame_header_.mi_rows + 31) >> 5)
                          : ((frame_header_.mi_rows + 15) >> 4);
  const int sb_shift = sequence_header_.use_128x128_superblock ? 5 : 4;
  const int sb_size = sb_shift + 2;
  const int max_tile_width_sb = kMaxTileWidth >> sb_size;
  int max_tile_area_sb = kMaxTileArea >> (2 * sb_size);
  const int min_log2_tile_cols = TileLog2(max_tile_width_sb, sb_cols);
  const int max_log2_tile_cols = TileLog2(1, std::min(sb_cols, kMaxTileCols));
  const int max_log2_tile_rows = TileLog2(1, std::min(sb_rows, kMaxTileRows));
  const int min_log2_tiles = std::max(
      min_log2_tile_cols, TileLog2(max_tile_area_sb, sb_rows * sb_cols));

  bool uniform_tile_spacing_flag = false;
  RCHECK(reader->ReadBits(1, &uniform_tile_spacing_flag));
  if (uniform_tile_spacing_flag) {
    tile_info.tile_cols_log2 = min_log2_tile_cols;
    while (tile_info.tile_cols_log2 < max_log2_tile_cols) {
      bool increment_tile_cols_log2 = false;
      RCHECK(reader->ReadBits(1, &increment_tile_cols_log2));
      if (increment_tile_cols_log2)
        tile_info.tile_cols_log2++;
      else
        break;
    }
    const int tile_width_sb = (sb_cols + (1 << tile_info.tile_cols_log2) - 1) >>
                              tile_info.tile_cols_log2;
    int i = 0;
    for (int start_sb = 0; start_sb < sb_cols; start_sb += tile_width_sb) {
      i += 1;
    }
    tile_info.tile_cols = i;

    const int min_log2_tile_rows =
        std::max(min_log2_tiles - tile_info.tile_cols_log2, 0);
    tile_info.tile_rows_log2 = min_log2_tile_rows;
    while (tile_info.tile_rows_log2 < max_log2_tile_rows) {
      bool increment_tile_rows_log2 = false;
      RCHECK(reader->ReadBits(1, &increment_tile_rows_log2));
      if (increment_tile_rows_log2)
        tile_info.tile_rows_log2++;
      else
        break;
    }
    const int tile_height_sb =
        (sb_rows + (1 << tile_info.tile_rows_log2) - 1) >>
        tile_info.tile_rows_log2;
    i = 0;
    for (int start_sb = 0; start_sb < sb_rows; start_sb += tile_height_sb) {
      i += 1;
    }
    tile_info.tile_rows = i;
  } else {
    int widest_tile_sb = 0;
    int start_sb = 0;
    int i = 0;
    for (; start_sb < sb_cols; i++) {
      const int max_width = std::min(sb_cols - start_sb, max_tile_width_sb);
      int width_in_sbs_minus_1 = 0;
      RCHECK(ReadNs(max_width, reader, &width_in_sbs_minus_1));
      const int size_sb = width_in_sbs_minus_1 + 1;
      widest_tile_sb = std::max(size_sb, widest_tile_sb);
      start_sb += size_sb;
    }
    tile_info.tile_cols = i;
    tile_info.tile_cols_log2 = TileLog2(1, tile_info.tile_cols);

    if (min_log2_tiles > 0)
      max_tile_area_sb = (sb_rows * sb_cols) >> (min_log2_tiles + 1);
    else
      max_tile_area_sb = sb_rows * sb_cols;
    const int max_tile_height_sb =
        std::max(max_tile_area_sb / widest_tile_sb, 1);

    start_sb = 0;
    i = 0;
    for (; start_sb < sb_rows; i++) {
      const int max_height = std::min(sb_rows - start_sb, max_tile_height_sb);
      int height_in_sbs_minus_1 = 0;
      RCHECK(ReadNs(max_height, reader, &height_in_sbs_minus_1));
      const int size_sb = height_in_sbs_minus_1 + 1;
      start_sb += size_sb;
    }
    tile_info.tile_rows = i;
    tile_info.tile_rows_log2 = TileLog2(1, tile_info.tile_rows);
  }
  if (tile_info.tile_cols_log2 > 0 || tile_info.tile_rows_log2 > 0) {
    // Skip context_update_tile_id.
    RCHECK(
        reader->SkipBits(tile_info.tile_rows_log2 + tile_info.tile_cols_log2));
    int tile_size_bytes_minus_1 = 0;
    RCHECK(reader->ReadBits(2, &tile_size_bytes_minus_1));
    tile_info.tile_size_bytes = tile_size_bytes_minus_1 + 1;
  }
  return true;
}

// 5.9.17. Quantizer index delta parameters syntax.
bool AV1Parser::SkipDeltaQParams(BitReader* reader, bool* delta_q_present) {
  *delta_q_present = false;
  if (frame_header_.quantization_params.base_q_idx > 0)
    RCHECK(reader->ReadBits(1, delta_q_present));
  if (*delta_q_present) {
    // Skip delta_q_res.
    RCHECK(reader->SkipBits(2));
  }
  return true;
}

// 5.9.18. Loop filter delta parameters syntax.
bool AV1Parser::SkipDeltaLfParams(bool delta_q_present,
                                  bool allow_intrabc,
                                  BitReader* reader) {
  bool delta_lf_present = false;
  if (delta_q_present) {
    if (!allow_intrabc)
      RCHECK(reader->ReadBits(1, &delta_lf_present));
    if (delta_lf_present) {
      // Skip delta_lf_res, delta_lf_multi.
      RCHECK(reader->SkipBits(2 + 1));
    }
  }
  return true;
}

// 5.9.19. CDEF params syntax.
bool AV1Parser::ParseCdefParams(bool coded_lossless,
                                bool allow_intrabc,
                                BitReader* reader) {
  if (coded_lossless || allow_intrabc || !sequence_header_.enable_cdef)
    return true;

  // Skip cdef_damping_minus_3.
  RCHECK(reader->SkipBits(2));
  int cdef_bits = 0;
  RCHECK(reader->ReadBits(2, &cdef_bits));
  for (int i = 0; i < (1 << cdef_bits); i++) {
    // Skip cdef_y_pri_strength[i], Skip cdef_y_sec_strength[i].
    RCHECK(reader->SkipBits(4 + 2));
    if (sequence_header_.color_config.num_planes > 1) {
      // Skip cdef_uv_pri_strength[i], Skip cdef_uv_sec_strength[i].
      RCHECK(reader->SkipBits(4 + 2));
    }
  }
  return true;
}

// 5.9.20. Loop restoration params syntax.
bool AV1Parser::ParseLrParams(bool all_lossless,
                              bool allow_intrabc,
                              BitReader* reader) {
  if (all_lossless || allow_intrabc || !sequence_header_.enable_restoration)
    return true;

  enum FrameRestorationType {
    RESTORE_NONE = 0,
    RESTORE_SWITCHABLE = 3,
    RESTORE_WIENER = 1,
    RESTORE_SGRPROJ = 2,
  };
  static const int kRemapLrType[4] = {RESTORE_NONE, RESTORE_SWITCHABLE,
                                      RESTORE_WIENER, RESTORE_SGRPROJ};
  bool uses_lr = false;
  bool uses_chroma_lr = false;
  for (int i = 0; i < sequence_header_.color_config.num_planes; i++) {
    int lr_type = 0;
    RCHECK(reader->ReadBits(2, &lr_type));
    const int frame_restoration_type = kRemapLrType[lr_type];
    if (frame_restoration_type != RESTORE_NONE) {
      uses_lr = true;
      if (i > 0)
        uses_chroma_lr = true;
    }
  }

  if (uses_lr) {
    if (sequence_header_.use_128x128_superblock) {
      // Skip lr_unit_shift.
      RCHECK(reader->SkipBits(1));
    } else {
      // Skip lr_unit_shift, lr_unit_extra_shift.
      RCHECK(reader->SkipBitsConditional(true, 1));
    }
    if (sequence_header_.color_config.subsampling_x &&
        sequence_header_.color_config.subsampling_y && uses_chroma_lr) {
      // Skip lr_uv_shift.
      RCHECK(reader->SkipBits(1));
    }
  }
  return true;
}

// 5.9.21. TX mode syntax.
bool AV1Parser::SkipTxMode(bool coded_lossless, BitReader* reader) {
  if (!coded_lossless) {
    // Skip tx_mode_select.
    RCHECK(reader->SkipBits(1));
  }
  return true;
}

// 5.9.22. Skip mode params syntax.
bool AV1Parser::SkipSkipModeParams(bool frame_is_intra,
                                   bool reference_select,
                                   BitReader* reader) {
  bool skip_mode_allowed = false;
  if (frame_is_intra || !reference_select ||
      !sequence_header_.enable_order_hint) {
    skip_mode_allowed = false;
  } else {
    int forward_idx = -1;
    int forward_hint = 0;
    int backward_idx = -1;
    int backward_hint = 0;
    for (int i = 0; i < kRefsPerFrame; i++) {
      const int ref_hint =
          reference_frames_[frame_header_.ref_frame_idx[i]].order_hint;
      if (GetRelativeDist(ref_hint, frame_header_.order_hint) < 0) {
        if (forward_idx < 0 || GetRelativeDist(ref_hint, forward_hint) > 0) {
          forward_idx = i;
          forward_hint = ref_hint;
        }
      } else if (GetRelativeDist(ref_hint, frame_header_.order_hint) > 0) {
        if (backward_idx < 0 || GetRelativeDist(ref_hint, backward_hint) < 0) {
          backward_idx = i;
          backward_hint = ref_hint;
        }
      }
    }
    if (forward_idx < 0) {
      skip_mode_allowed = false;
    } else if (backward_idx >= 0) {
      skip_mode_allowed = true;
    } else {
      int second_forward_idx = -1;
      int second_forward_hint = 0;
      for (int i = 0; i < kRefsPerFrame; i++) {
        const int ref_hint =
            reference_frames_[frame_header_.ref_frame_idx[i]].order_hint;
        if (GetRelativeDist(ref_hint, forward_hint) < 0) {
          if (second_forward_idx < 0 ||
              GetRelativeDist(ref_hint, second_forward_hint) > 0) {
            second_forward_idx = i;
            second_forward_hint = ref_hint;
          }
        }
      }
      skip_mode_allowed = second_forward_idx >= 0;
    }
  }

  if (skip_mode_allowed) {
    // Skip skip_mode_present.
    RCHECK(reader->SkipBits(1));
  }
  return true;
}

// 5.9.23. Frame reference mode syntax.
bool AV1Parser::ParseFrameReferenceMode(bool frame_is_intra,
                                        BitReader* reader,
                                        bool* reference_select) {
  if (frame_is_intra)
    *reference_select = false;
  else
    RCHECK(reader->ReadBits(1, reference_select));
  return true;
}

// 5.9.24. Global motion params syntax.
bool AV1Parser::SkipGlobalMotionParams(bool frame_is_intra,
                                       bool allow_high_precision_mv,
                                       BitReader* reader) {
  if (frame_is_intra)
    return true;

  for (int ref = LAST_FRAME; ref <= ALTREF_FRAME; ref++) {
    int type = 0;

    bool is_global = false;
    RCHECK(reader->ReadBits(1, &is_global));
    if (is_global) {
      bool is_rot_zoom = false;
      RCHECK(reader->ReadBits(1, &is_rot_zoom));
      if (is_rot_zoom) {
        type = ROTZOOM;
      } else {
        bool is_translation = false;
        RCHECK(reader->ReadBits(1, &is_translation));
        type = is_translation ? TRANSLATION : AFFINE;
      }
    } else {
      type = IDENTITY;
    }

    if (type >= ROTZOOM) {
      RCHECK(SkipGlobalParam(type, ref, 2, allow_high_precision_mv, reader));
      RCHECK(SkipGlobalParam(type, ref, 3, allow_high_precision_mv, reader));
      if (type == AFFINE) {
        RCHECK(SkipGlobalParam(type, ref, 4, allow_high_precision_mv, reader));
        RCHECK(SkipGlobalParam(type, ref, 5, allow_high_precision_mv, reader));
      }
    }
    if (type >= TRANSLATION) {
      RCHECK(SkipGlobalParam(type, ref, 0, allow_high_precision_mv, reader));
      RCHECK(SkipGlobalParam(type, ref, 1, allow_high_precision_mv, reader));
    }
  }
  return true;
}

// 5.9.25. Global param syntax.
bool AV1Parser::SkipGlobalParam(int type,
                                int /*ref*/,
                                int idx,
                                bool allow_high_precision_mv,
                                BitReader* reader) {
  const int kGmAbsTransBits = 12;
  const int kGmAbsTransOnlyBits = 9;
  const int kGmAbsAlphaBits = 12;

  int abs_bits = kGmAbsAlphaBits;
  if (idx < 2) {
    if (type == TRANSLATION) {
      abs_bits = kGmAbsTransOnlyBits - (allow_high_precision_mv ? 0 : 1);
    } else {
      abs_bits = kGmAbsTransBits;
    }
  }
  const int mx = 1 << abs_bits;
  RCHECK(SkipDecodeSignedSubexpWithRef(-mx, mx + 1, reader));
  return true;
}

// 5.9.26. Decode signed subexp with ref syntax.
bool AV1Parser::SkipDecodeSignedSubexpWithRef(int low,
                                              int high,
                                              BitReader* reader) {
  RCHECK(SkipDecodeUnsignedSubexpWithRef(high - low, reader));
  return true;
}

// 5.9.27. Decode unsigned subbexp with ref syntax.
bool AV1Parser::SkipDecodeUnsignedSubexpWithRef(int mx, BitReader* reader) {
  RCHECK(SkipDecodeSubexp(mx, reader));
  return true;
}

// 5.9.28. Decode subexp syntax.
bool AV1Parser::SkipDecodeSubexp(int num_syms, BitReader* reader) {
  int i = 0;
  int mk = 0;
  int k = 3;
  while (true) {
    const int b2 = i ? (k + i - 1) : k;
    const int a = 1 << b2;
    if (num_syms <= mk + 3 * a) {
      int subexp_final_bits = 0;
      RCHECK(ReadNs(num_syms - mk, reader, &subexp_final_bits));
      return true;
    } else {
      bool subexp_more_bits = false;
      RCHECK(reader->ReadBits(1, &subexp_more_bits));
      if (subexp_more_bits) {
        i++;
        mk += a;
      } else {
        // Skip subexp_bits.
        RCHECK(reader->SkipBits(b2));
        return true;
      }
    }
  }
  return true;
}

// 5.9.30. Film grain params syntax.
bool AV1Parser::SkipFilmGrainParams(bool show_frame,
                                    bool showable_frame,
                                    BitReader* reader) {
  if (!sequence_header_.film_grain_params_present ||
      (!show_frame && !showable_frame)) {
    return true;
  }

  bool apply_grain = false;
  RCHECK(reader->ReadBits(1, &apply_grain));
  if (!apply_grain)
    return true;

  // Skip grain_seed.
  RCHECK(reader->SkipBits(16));
  bool update_grain = true;
  if (frame_header_.frame_type == INTER_FRAME)
    RCHECK(reader->ReadBits(1, &update_grain));
  if (!update_grain) {
    // Skip film_grain_params_ref_idx.
    RCHECK(reader->SkipBits(3));
    return true;
  }

  int num_y_points = 0;
  RCHECK(reader->ReadBits(4, &num_y_points));
  // Skip point_y_value, point_y_scaling.
  RCHECK(reader->SkipBits((8 + 8) * num_y_points));

  const ColorConfig& color_config = sequence_header_.color_config;
  bool chroma_scaling_from_luma = false;
  if (!color_config.mono_chrome)
    RCHECK(reader->ReadBits(1, &chroma_scaling_from_luma));
  int num_cb_points = 0;
  int num_cr_points = 0;
  if (color_config.mono_chrome || chroma_scaling_from_luma ||
      (color_config.subsampling_x && color_config.subsampling_y &&
       num_y_points == 0)) {
    num_cb_points = 0;
    num_cr_points = 0;
  } else {
    RCHECK(reader->ReadBits(4, &num_cb_points));
    // Skip point_cb_value, point_cb_scaling.
    RCHECK(reader->SkipBits((8 + 8) * num_cb_points));
    RCHECK(reader->ReadBits(4, &num_cr_points));
    // Skip point_cr_value, point_cr_scaling.
    RCHECK(reader->SkipBits((8 + 8) * num_cr_points));
  }

  // Skip grain_scaling_minus_8.
  RCHECK(reader->SkipBits(2));
  int ar_coeff_lag = 0;
  RCHECK(reader->ReadBits(2, &ar_coeff_lag));
  const int num_pos_luma = 2 * ar_coeff_lag * (ar_coeff_lag + 1);
  int num_pos_chroma = num_pos_luma;
  if (num_y_points) {
    num_pos_chroma = num_pos_luma + 1;
    // Skip ar_coeffs_y_plus_128.
    RCHECK(reader->SkipBits(8 * num_pos_luma));
  }
  if (chroma_scaling_from_luma || num_cb_points) {
    // Skip ar_coeffs_cb_plus_128.
    RCHECK(reader->SkipBits(8 * num_pos_chroma));
  }
  if (chroma_scaling_from_luma || num_cr_points) {
    // Skip ar_coeffs_cb_plus_128.
    RCHECK(reader->SkipBits(8 * num_pos_chroma));
  }

  // Skip ar_coeff_shift_minus_6, grain_scale_shift.
  RCHECK(reader->SkipBits(2 + 2));
  if (num_cb_points) {
    // Skip cb_mult, cb_luma_mult, cb_offset.
    RCHECK(reader->SkipBits(8 + 8 + 9));
  }
  if (num_cr_points) {
    // Skip cr_mult, cr_luma_mult, cr_offset.
    RCHECK(reader->SkipBits(8 + 8 + 9));
  }
  // Skip overlap_flag, clip_restricted_range.
  RCHECK(reader->SkipBits(1 + 1));
  return true;
}

// 5.9.31. Temporal point info syntax.
bool AV1Parser::SkipTemporalPointInfo(BitReader* reader) {
  const int frame_presentation_time_length =
      sequence_header_.decoder_model_info
          .frame_presentation_time_length_minus_1 +
      1;
  // Skip frame_presentation_time.
  RCHECK(reader->SkipBits(frame_presentation_time_length));
  return true;
}

// 5.10. Frame OBU syntax.
bool AV1Parser::ParseFrameObu(const ObuHeader& obu_header,
                              size_t size,
                              BitReader* reader,
                              std::vector<Tile>* tiles) {
  const size_t start_bit_pos = reader->bit_position();
  RCHECK(ParseFrameHeaderObu(obu_header, reader));
  RCHECK(ByteAlignment(reader));
  const size_t end_bit_pos = reader->bit_position();
  const size_t header_bytes = (end_bit_pos - start_bit_pos) / 8;
  RCHECK(ParseTileGroupObu(size - header_bytes, reader, tiles));
  return true;
}

// 5.11.1. General tile group OBU syntax.
bool AV1Parser::ParseTileGroupObu(size_t size,
                                  BitReader* reader,
                                  std::vector<Tile>* tiles) {
  const TileInfo& tile_info = frame_header_.tile_info;
  const size_t start_bit_pos = reader->bit_position();

  const int num_tiles = tile_info.tile_cols * tile_info.tile_rows;
  bool tile_start_and_end_present_flag = false;
  if (num_tiles > 1)
    RCHECK(reader->ReadBits(1, &tile_start_and_end_present_flag));

  int tg_start = 0;
  int tg_end = num_tiles - 1;
  if (num_tiles > 1 && tile_start_and_end_present_flag) {
    const int tile_bits = tile_info.tile_cols_log2 + tile_info.tile_rows_log2;
    RCHECK(reader->ReadBits(tile_bits, &tg_start));
    RCHECK(reader->ReadBits(tile_bits, &tg_end));
  }
  RCHECK(ByteAlignment(reader));

  const size_t end_bit_pos = reader->bit_position();
  const size_t header_bytes = (end_bit_pos - start_bit_pos) / 8;
  size -= header_bytes;

  for (int tile_num = tg_start; tile_num <= tg_end; tile_num++) {
    const bool last_tile = tile_num == tg_end;
    size_t tile_size = size;
    if (!last_tile) {
      size_t tile_size_minus_1 = 0;
      RCHECK(ReadLe(tile_info.tile_size_bytes, reader, &tile_size_minus_1));
      tile_size = tile_size_minus_1 + 1;
      size -= tile_size + tile_info.tile_size_bytes;
    }
    tiles->push_back({reader->bit_position() / 8, tile_size});
    RCHECK(reader->SkipBits(tile_size * 8));  // Skip the tile.
  }

  if (tg_end == num_tiles - 1) {
    DecodeFrameWrapup();
    frame_header_.seen_frame_header = false;
  }
  return true;
}

// 5.11.14. Segmentation feature active function.
bool AV1Parser::SegFeatureActiveIdx(int idx, int feature) {
  const SegmentationParams& segmentation_params =
      frame_header_.segmentation_params;
  return segmentation_params.segmentation_enabled &&
         segmentation_params.feature_enabled[idx][feature];
}

// 7.4. Decode frame wrapup process.
void AV1Parser::DecodeFrameWrapup() {
  const int refresh_frame_flags = frame_header_.refresh_frame_flags;
  if (frame_header_.show_existing_frame &&
      frame_header_.frame_type == KEY_FRAME) {
    // 7.21. Reference frame loading process.
    const ReferenceFrame& reference_frame =
        reference_frames_[frame_header_.frame_to_show_map_idx];

    frame_header_.upscaled_width = reference_frame.upscaled_width;
    frame_header_.frame_width = reference_frame.frame_width;
    frame_header_.frame_height = reference_frame.frame_height;
    frame_header_.render_width = reference_frame.render_width;
    frame_header_.render_height = reference_frame.render_height;
    frame_header_.mi_cols = reference_frame.mi_cols;
    frame_header_.mi_rows = reference_frame.mi_rows;

    ColorConfig& color_config = sequence_header_.color_config;
    color_config.subsampling_x = reference_frame.subsampling_x;
    color_config.subsampling_y = reference_frame.subsampling_y;
    color_config.bit_depth = reference_frame.bit_depth;

    frame_header_.order_hint = reference_frame.order_hint;
  }
  // 7.20. Reference frame update process.
  for (int i = 0; i <= kNumRefFrames - 1; i++) {
    if ((refresh_frame_flags >> i) & 1) {
      ReferenceFrame& reference_frame = reference_frames_[i];

      reference_frame.upscaled_width = frame_header_.upscaled_width;
      reference_frame.frame_width = frame_header_.frame_width;
      reference_frame.frame_height = frame_header_.frame_height;
      reference_frame.render_width = frame_header_.render_width;
      reference_frame.render_height = frame_header_.render_height;
      reference_frame.mi_cols = frame_header_.mi_cols;
      reference_frame.mi_rows = frame_header_.mi_rows;
      reference_frame.frame_type = frame_header_.frame_type;

      const ColorConfig& color_config = sequence_header_.color_config;
      reference_frame.subsampling_x = color_config.subsampling_x;
      reference_frame.subsampling_y = color_config.subsampling_y;
      reference_frame.bit_depth = color_config.bit_depth;

      reference_frame.order_hint = frame_header_.order_hint;
    }
  }
}

// 7.8. Set frame refs process.
bool AV1Parser::SetFrameRefs(int last_frame_idx, int gold_frame_idx) {
  for (int i = 0; i < kRefsPerFrame; i++)
    frame_header_.ref_frame_idx[i] = -1;
  frame_header_.ref_frame_idx[LAST_FRAME - LAST_FRAME] = last_frame_idx;
  frame_header_.ref_frame_idx[GOLDEN_FRAME - LAST_FRAME] = gold_frame_idx;

  bool used_frame[kNumRefFrames] = {};
  used_frame[last_frame_idx] = true;
  used_frame[gold_frame_idx] = true;

  const int cur_frame_hint = 1 << (sequence_header_.order_hint_bits - 1);

  // An array containing the expected output order shifted such that the
  // current frame has hint equal to |cur_frame_hint| is prepared.
  int shifted_order_hints[kNumRefFrames];
  for (int i = 0; i < kNumRefFrames; i++) {
    shifted_order_hints[i] =
        cur_frame_hint + GetRelativeDist(reference_frames_[i].order_hint,
                                         frame_header_.order_hint);
  }

  const int last_order_hint = shifted_order_hints[last_frame_idx];
  RCHECK(last_order_hint < cur_frame_hint);
  const int gold_order_hint = shifted_order_hints[gold_frame_idx];
  RCHECK(gold_order_hint < cur_frame_hint);

  // The ALTREF_FRAME reference is set to be a backward reference to the frame
  // with highest output order.
  int ref = FindLatestBackward(shifted_order_hints, used_frame, cur_frame_hint);
  if (ref >= 0) {
    frame_header_.ref_frame_idx[ALTREF_FRAME - LAST_FRAME] = ref;
    used_frame[ref] = true;
  }

  // The BWDREF_FRAME reference is set to be a backward reference to the cloest
  // frame.
  ref = FindEarliestBackward(shifted_order_hints, used_frame, cur_frame_hint);
  if (ref >= 0) {
    frame_header_.ref_frame_idx[BWDREF_FRAME - LAST_FRAME] = ref;
    used_frame[ref] = true;
  }

  // The ALTREF2_FRAME reference is set to the next closest backward reference.
  ref = FindEarliestBackward(shifted_order_hints, used_frame, cur_frame_hint);
  if (ref >= 0) {
    frame_header_.ref_frame_idx[ALTREF2_FRAME - LAST_FRAME] = ref;
    used_frame[ref] = true;
  }

  // The remaining references are set to be forward references in
  // anti-chronological order.
  static const int kRefFrameList[] = {
      LAST2_FRAME, LAST3_FRAME, BWDREF_FRAME, ALTREF2_FRAME, ALTREF_FRAME,
  };
  static_assert(std::size(kRefFrameList) == kRefsPerFrame - 2,
                "Unexpected kRefFrameList size.");
  for (const int ref_frame : kRefFrameList) {
    if (frame_header_.ref_frame_idx[ref_frame - LAST_FRAME] < 0) {
      ref = FindLatestForward(shifted_order_hints, used_frame, cur_frame_hint);
      if (ref >= 0) {
        frame_header_.ref_frame_idx[ref_frame - LAST_FRAME] = ref;
        used_frame[ref] = true;
      }
    }
  }

  // Finally, any remaining references are set to the reference frame with
  // smallest output order.
  ref = -1;
  int earliest_order_hint = 0;
  for (int i = 0; i < kNumRefFrames; i++) {
    const int hint = shifted_order_hints[i];
    if (ref < 0 || hint < earliest_order_hint) {
      ref = i;
      earliest_order_hint = hint;
    }
  }
  for (int i = 0; i < kRefsPerFrame; i++) {
    if (frame_header_.ref_frame_idx[i] < 0) {
      frame_header_.ref_frame_idx[i] = ref;
    }
  }

  return true;
}

// 7.12.2. Dequantization functions. The function returns the quantizer index
// for the current block.
int AV1Parser::GetQIndex(bool ignore_delta_q, int segment_id) {
  // We do not have use case for ignore_delta_q false case.
  CHECK(ignore_delta_q) << "ignoreDeltaQ equal to 0 is not supported.";

  const int base_q_idx = frame_header_.quantization_params.base_q_idx;

  const int kSegLvlAltQ = 0;
  if (SegFeatureActiveIdx(segment_id, kSegLvlAltQ)) {
    const int data =
        frame_header_.segmentation_params.feature_data[segment_id][kSegLvlAltQ];
    const int qindex = base_q_idx + data;
    return Clip3(0, 255, qindex);
  } else {
    return base_q_idx;
  }
}

}  // namespace media
}  // namespace shaka
