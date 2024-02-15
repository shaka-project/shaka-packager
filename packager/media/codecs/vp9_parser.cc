// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/vp9_parser.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/base/rcheck.h>

namespace shaka {
namespace media {
namespace {

const uint32_t VP9_FRAME_MARKER = 2;
const uint32_t VP9_SYNC_CODE = 0x498342;
const uint32_t REFS_PER_FRAME = 3;
const uint32_t REF_FRAMES_LOG2 = 3;
const uint32_t REF_FRAMES = (1 << REF_FRAMES_LOG2);
const uint32_t FRAME_CONTEXTS_LOG2 = 2;
const uint32_t MAX_REF_LF_DELTAS = 4;
const uint32_t MAX_MODE_LF_DELTAS = 2;
const uint32_t QINDEX_BITS = 8;
const uint32_t MAX_SEGMENTS = 8;
const uint32_t SEG_TREE_PROBS = (MAX_SEGMENTS - 1);
const uint32_t PREDICTION_PROBS = 3;
const uint32_t SEG_LVL_MAX = 4;
const uint32_t MI_SIZE_LOG2 = 3;
const uint32_t MI_BLOCK_SIZE_LOG2 = (6 - MI_SIZE_LOG2);  // 64 = 2^6
const uint32_t MIN_TILE_WIDTH_B64 = 4;
const uint32_t MAX_TILE_WIDTH_B64 = 64;

const bool SEG_FEATURE_DATA_SIGNED[SEG_LVL_MAX] = {true, true, false, false};
const uint32_t SEG_FEATURE_DATA_MAX_BITS[SEG_LVL_MAX] = {8, 6, 2, 0};

enum VpxColorSpace {
  VPX_COLOR_SPACE_UNKNOWN = 0,
  VPX_COLOR_SPACE_BT_601 = 1,
  VPX_COLOR_SPACE_BT_709 = 2,
  VPX_COLOR_SPACE_SMPTE_170 = 3,
  VPX_COLOR_SPACE_SMPTE_240 = 4,
  VPX_COLOR_SPACE_BT_2020 = 5,
  VPX_COLOR_SPACE_RESERVED = 6,
  VPX_COLOR_SPACE_SRGB = 7,
};

uint32_t RoundupShift(uint32_t value, uint32_t n) {
  return (value + (1 << n) - 1) >> n;
}

// Number of MI-units (8*8).
uint32_t GetNumMiUnits(uint32_t pixels) {
  return RoundupShift(pixels, MI_SIZE_LOG2);
}

// Number of sb64 (64x64) blocks per mi_units.
uint32_t GetNumBlocks(uint32_t mi_units) {
  return RoundupShift(mi_units, MI_BLOCK_SIZE_LOG2);
}

uint32_t GetMinLog2TileCols(uint32_t sb64_cols) {
  uint32_t min_log2 = 0;
  while ((MAX_TILE_WIDTH_B64 << min_log2) < sb64_cols)
    ++min_log2;
  return min_log2;
}

uint32_t GetMaxLog2TileCols(uint32_t sb64_cols) {
  uint32_t max_log2 = 1;
  while ((sb64_cols >> max_log2) >= MIN_TILE_WIDTH_B64)
    ++max_log2;
  return max_log2 - 1;
}

void GetTileNBits(uint32_t mi_cols,
                  uint32_t* min_log2_tile_cols,
                  uint32_t* max_log2_tile_cols) {
  const uint32_t sb64_cols = GetNumBlocks(mi_cols);
  *min_log2_tile_cols = GetMinLog2TileCols(sb64_cols);
  *max_log2_tile_cols = GetMaxLog2TileCols(sb64_cols);
  CHECK_LE(*min_log2_tile_cols, *max_log2_tile_cols);
}

// Parse superframe index if it is a superframe. Fill |vpx_frames| with the
// frames information, which contains the sizes of the frames indicated in
// superframe index if it is a superframe; otherwise it should contain one
// single frame with |data_size| as frame size.
bool ParseIfSuperframeIndex(const uint8_t* data,
                            size_t data_size,
                            std::vector<VPxFrameInfo>* vpx_frames) {
  vpx_frames->clear();
  uint8_t superframe_marker = data[data_size - 1];
  VPxFrameInfo vpx_frame;
  if ((superframe_marker & 0xe0) != 0xc0) {
    // This is not a super frame. There should be only one frame.
    vpx_frame.frame_size = data_size;
    vpx_frames->push_back(vpx_frame);
    return true;
  }

  const size_t num_frames = (superframe_marker & 0x07) + 1;
  const size_t frame_size_length = ((superframe_marker >> 3) & 0x03) + 1;
  // Two maker bytes + frame sizes.
  const size_t index_size = 2 + num_frames * frame_size_length;

  if (data_size < index_size) {
    LOG(ERROR) << "This chunk is marked as having a superframe index but "
                  "doesn't have enough data for it.";
    return false;
  }
  const uint8_t superframe_marker2 = data[data_size - index_size];
  if (superframe_marker2 != superframe_marker) {
    LOG(ERROR) << "This chunk is marked as having a superframe index but "
                  "doesn't have the matching marker byte at the front of the "
                  "index.";
    return false;
  }
  VLOG(3) << "Superframe num_frames=" << num_frames
          << " frame_size_length=" << frame_size_length;

  data += data_size - index_size + 1;
  size_t total_frame_sizes = 0;
  for (size_t frame = 0; frame < num_frames; ++frame) {
    vpx_frame.frame_size = 0;
    for (size_t i = 0; i < frame_size_length; ++i) {
      vpx_frame.frame_size |= *data << (i * 8);
      ++data;
    }
    total_frame_sizes += vpx_frame.frame_size;
    vpx_frames->push_back(vpx_frame);
  }
  if (total_frame_sizes + index_size != data_size) {
    LOG(ERROR) << "Data size (" << data_size
               << ") does not match with sum of frame sizes ("
               << total_frame_sizes << ") + index_size (" << index_size << ")";
    return false;
  }
  return true;
}

bool ReadProfile(BitReader* reader, VPCodecConfigurationRecord* codec_config) {
  uint8_t bit[2];
  RCHECK(reader->ReadBits(1, &bit[0]));
  RCHECK(reader->ReadBits(1, &bit[1]));
  uint8_t profile = bit[0] | (bit[1] << 1);
  if (profile == 3) {
    bool reserved;
    RCHECK(reader->ReadBits(1, &reserved));
    RCHECK(!reserved);
  }
  codec_config->set_profile(profile);
  return true;
}

bool ReadSyncCode(BitReader* reader) {
  uint32_t sync_code;
  RCHECK(reader->ReadBits(24, &sync_code));
  return sync_code == VP9_SYNC_CODE;
}

void SetColorAttributes(uint8_t bit_depth,
                        uint8_t color_space,
                        VPCodecConfigurationRecord* codec_config) {
  switch (color_space) {
    case VPX_COLOR_SPACE_UNKNOWN:
      codec_config->set_color_primaries(AVCOL_PRI_UNSPECIFIED);
      codec_config->set_matrix_coefficients(AVCOL_SPC_UNSPECIFIED);
      codec_config->set_transfer_characteristics(AVCOL_TRC_UNSPECIFIED);
      break;
    case VPX_COLOR_SPACE_BT_601:
      // Don't know if it is 525 line or 625 line.
      codec_config->set_color_primaries(AVCOL_PRI_UNSPECIFIED);
      codec_config->set_matrix_coefficients(AVCOL_SPC_UNSPECIFIED);
      codec_config->set_transfer_characteristics(AVCOL_TRC_SMPTE170M);
      break;
    case VPX_COLOR_SPACE_BT_709:
      codec_config->set_color_primaries(AVCOL_PRI_BT709);
      codec_config->set_matrix_coefficients(AVCOL_SPC_BT709);
      codec_config->set_transfer_characteristics(AVCOL_TRC_BT709);
      break;
    case VPX_COLOR_SPACE_SMPTE_170:
      codec_config->set_color_primaries(AVCOL_PRI_SMPTE170M);
      codec_config->set_matrix_coefficients(AVCOL_SPC_SMPTE170M);
      codec_config->set_transfer_characteristics(AVCOL_TRC_SMPTE170M);
      break;
    case VPX_COLOR_SPACE_SMPTE_240:
      codec_config->set_color_primaries(AVCOL_PRI_SMPTE240M);
      codec_config->set_matrix_coefficients(AVCOL_SPC_SMPTE240M);
      codec_config->set_transfer_characteristics(AVCOL_TRC_SMPTE240M);
      break;
    case VPX_COLOR_SPACE_BT_2020:
      codec_config->set_color_primaries(AVCOL_PRI_BT2020);
      // VP9 does not specify if it is in the form of “constant luminance” or
      // “non-constant luminance”. As such, application should rely on the
      // signaling outside of VP9 bitstream. If there is no such signaling,
      // application may assume non-constant luminance for BT.2020.
      codec_config->set_matrix_coefficients(AVCOL_SPC_BT2020_NCL);
      switch (bit_depth) {
        case 10:
          codec_config->set_transfer_characteristics(AVCOL_TRC_BT2020_10);
          break;
        case 12:
          codec_config->set_transfer_characteristics(AVCOL_TRC_BT2020_12);
          break;
        default:
          codec_config->set_transfer_characteristics(AVCOL_TRC_UNSPECIFIED);
          break;
      }
      break;
    case VPX_COLOR_SPACE_SRGB:
      codec_config->set_color_primaries(AVCOL_PRI_UNSPECIFIED);
      codec_config->set_matrix_coefficients(AVCOL_SPC_RGB);
      codec_config->set_transfer_characteristics(AVCOL_TRC_UNSPECIFIED);
      break;
    default:
      LOG(WARNING) << "Unknown color space: " << static_cast<int>(color_space);
      codec_config->set_color_primaries(AVCOL_PRI_UNSPECIFIED);
      codec_config->set_matrix_coefficients(AVCOL_SPC_UNSPECIFIED);
      codec_config->set_transfer_characteristics(AVCOL_TRC_UNSPECIFIED);
      break;
  }
}

VPCodecConfigurationRecord::ChromaSubsampling GetChromaSubsampling(
    uint8_t subsampling) {
  switch (subsampling) {
    case 0:
      return VPCodecConfigurationRecord::CHROMA_444;
    case 1:
      return VPCodecConfigurationRecord::CHROMA_440;
    case 2:
      return VPCodecConfigurationRecord::CHROMA_422;
    case 3:
      // VP9 assumes that chrome samples are collocated with luma samples if
      // there is no explicit signaling outside of VP9 bitstream.
      return VPCodecConfigurationRecord::CHROMA_420_COLLOCATED_WITH_LUMA;
    default:
      LOG(WARNING) << "Unexpected chroma subsampling value: "
                   << static_cast<int>(subsampling);
      return VPCodecConfigurationRecord::CHROMA_420_COLLOCATED_WITH_LUMA;
  }
}

bool ReadBitDepthAndColorSpace(BitReader* reader,
                               VPCodecConfigurationRecord* codec_config) {
  uint8_t bit_depth = 8;
  if (codec_config->profile() >= 2) {
    bool use_vpx_bits_12;
    RCHECK(reader->ReadBits(1, &use_vpx_bits_12));
    bit_depth = use_vpx_bits_12 ? 12 : 10;
  }
  codec_config->set_bit_depth(bit_depth);

  uint8_t color_space;
  RCHECK(reader->ReadBits(3, &color_space));
  SetColorAttributes(bit_depth, color_space, codec_config);

  bool yuv_full_range = false;
  auto chroma_subsampling = VPCodecConfigurationRecord::CHROMA_444;
  if (color_space != VPX_COLOR_SPACE_SRGB) {
    RCHECK(reader->ReadBits(1, &yuv_full_range));

    if (codec_config->profile() & 1) {
      uint8_t subsampling;
      RCHECK(reader->ReadBits(2, &subsampling));
      chroma_subsampling = GetChromaSubsampling(subsampling);
      if (chroma_subsampling ==
          VPCodecConfigurationRecord::CHROMA_420_COLLOCATED_WITH_LUMA) {
        LOG(ERROR) << "4:2:0 color not supported in profile "
                   << static_cast<int>(codec_config->profile());
        return false;
      }

      bool reserved;
      RCHECK(reader->ReadBits(1, &reserved));
      RCHECK(!reserved);
    } else {
      chroma_subsampling =
          VPCodecConfigurationRecord::CHROMA_420_COLLOCATED_WITH_LUMA;
    }
  } else {
    // Assume 4:4:4 for colorspace SRGB.
    chroma_subsampling = VPCodecConfigurationRecord::CHROMA_444;
    if (codec_config->profile() & 1) {
      bool reserved;
      RCHECK(reader->ReadBits(1, &reserved));
      RCHECK(!reserved);
    } else {
      LOG(ERROR) << "4:4:4 color not supported in profile 0 or 2.";
      return false;
    }
  }
  codec_config->set_video_full_range_flag(yuv_full_range);
  codec_config->SetChromaSubsampling(chroma_subsampling);

  VLOG(3) << "\n profile " << static_cast<int>(codec_config->profile())
          << "\n bit depth " << static_cast<int>(codec_config->bit_depth())
          << "\n matrix coefficients "
          << static_cast<int>(codec_config->matrix_coefficients())
          << "\n full_range "
          << static_cast<int>(codec_config->video_full_range_flag())
          << "\n chroma subsampling "
          << static_cast<int>(codec_config->chroma_subsampling());
  return true;
}

bool ReadFrameSize(BitReader* reader, uint32_t* width, uint32_t* height) {
  RCHECK(reader->ReadBits(16, width));
  *width += 1;  // Off by 1.
  RCHECK(reader->ReadBits(16, height));
  *height += 1;  // Off by 1.
  return true;
}

bool ReadDisplayFrameSize(BitReader* reader,
                          uint32_t* display_width,
                          uint32_t* display_height) {
  bool has_display_size;
  RCHECK(reader->ReadBits(1, &has_display_size));
  if (has_display_size)
    RCHECK(ReadFrameSize(reader, display_width, display_height));
  return true;
}

bool ReadFrameSizes(BitReader* reader, uint32_t* width, uint32_t* height) {
  uint32_t new_width;
  uint32_t new_height;
  RCHECK(ReadFrameSize(reader, &new_width, &new_height));
  if (new_width != *width) {
    VLOG(1) << "Width updates from " << *width << " to " << new_width;
    *width = new_width;
  }
  if (new_height != *height) {
    VLOG(1) << "Height updates from " << *height << " to " << new_height;
    *height = new_height;
  }

  uint32_t display_width = *width;
  uint32_t display_height = *height;
  RCHECK(ReadDisplayFrameSize(reader, &display_width, &display_height));
  return true;
}

bool ReadFrameSizesWithRefs(BitReader* reader,
                            uint32_t* width,
                            uint32_t* height) {
  bool found = false;
  for (uint32_t i = 0; i < REFS_PER_FRAME; ++i) {
    RCHECK(reader->ReadBits(1, &found));
    if (found)
      break;
  }
  if (!found) {
    RCHECK(ReadFrameSizes(reader, width, height));
  } else {
    uint32_t display_width;
    uint32_t display_height;
    RCHECK(ReadDisplayFrameSize(reader, &display_width, &display_height));
  }
  return true;
}

bool ReadLoopFilter(BitReader* reader) {
  RCHECK(reader->SkipBits(9));  // filter_evel, sharness_level
  bool mode_ref_delta_enabled;
  RCHECK(reader->ReadBits(1, &mode_ref_delta_enabled));
  if (!mode_ref_delta_enabled)
    return true;
  bool mode_ref_delta_update;
  RCHECK(reader->ReadBits(1, &mode_ref_delta_update));
  if (!mode_ref_delta_update)
    return true;

  for (uint32_t i = 0; i < MAX_REF_LF_DELTAS + MAX_MODE_LF_DELTAS; ++i)
    RCHECK(reader->SkipBitsConditional(true, 6 + 1));
  return true;
}

bool ReadQuantization(BitReader* reader) {
  RCHECK(reader->SkipBits(QINDEX_BITS));
  // Skip delta_q bits.
  for (uint32_t i = 0; i < 3; ++i)
    RCHECK(reader->SkipBitsConditional(true, 4 + 1));
  return true;
}

bool ReadSegmentation(BitReader* reader) {
  bool enabled;
  RCHECK(reader->ReadBits(1, &enabled));
  if (!enabled)
    return true;

  bool update_map;
  RCHECK(reader->ReadBits(1, &update_map));
  if (update_map) {
    for (uint32_t i = 0; i < SEG_TREE_PROBS; ++i)
      RCHECK(reader->SkipBitsConditional(true, 8));

    bool temporal_update;
    RCHECK(reader->ReadBits(1, &temporal_update));
    if (temporal_update) {
      for (uint32_t j = 0; j < PREDICTION_PROBS; ++j)
        RCHECK(reader->SkipBitsConditional(true, 8));
    }
  }

  bool update_data;
  RCHECK(reader->ReadBits(1, &update_data));
  if (update_data) {
    RCHECK(reader->SkipBits(1));  // abs_delta
    for (uint32_t i = 0; i < MAX_SEGMENTS; ++i) {
      for (uint32_t j = 0; j < SEG_LVL_MAX; ++j) {
        bool feature_enabled;
        RCHECK(reader->ReadBits(1, &feature_enabled));
        if (feature_enabled) {
          RCHECK(reader->SkipBits(SEG_FEATURE_DATA_MAX_BITS[j]));
          if (SEG_FEATURE_DATA_SIGNED[j])
            RCHECK(reader->SkipBits(1));  // signness
        }
      }
    }
  }
  return true;
}

bool ReadTileInfo(uint32_t width, BitReader* reader) {
  uint32_t mi_cols = GetNumMiUnits(width);

  uint32_t min_log2_tile_cols;
  uint32_t max_log2_tile_cols;
  GetTileNBits(mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);
  uint32_t max_ones = max_log2_tile_cols - min_log2_tile_cols;

  uint32_t log2_tile_cols = min_log2_tile_cols;
  while (max_ones--) {
    bool has_more;
    RCHECK(reader->ReadBits(1, &has_more));
    if (!has_more)
      break;
    ++log2_tile_cols;
  }
  RCHECK(log2_tile_cols <= 6);

  RCHECK(reader->SkipBitsConditional(true, 1));  // log2_tile_rows
  return true;
}

}  // namespace

VP9Parser::VP9Parser() : width_(0), height_(0) {}
VP9Parser::~VP9Parser() {}

bool VP9Parser::Parse(const uint8_t* data,
                      size_t data_size,
                      std::vector<VPxFrameInfo>* vpx_frames) {
  DCHECK(data);
  DCHECK(vpx_frames);
  RCHECK(ParseIfSuperframeIndex(data, data_size, vpx_frames));

  for (auto& vpx_frame : *vpx_frames) {
    VLOG(4) << "process frame with size " << vpx_frame.frame_size;
    BitReader reader(data, vpx_frame.frame_size);
    uint8_t frame_marker;
    RCHECK(reader.ReadBits(2, &frame_marker));
    RCHECK(frame_marker == VP9_FRAME_MARKER);

    RCHECK(ReadProfile(&reader, writable_codec_config()));

    bool show_existing_frame;
    RCHECK(reader.ReadBits(1, &show_existing_frame));
    if (show_existing_frame) {
      RCHECK(reader.SkipBits(3));  // ref_frame_index
      // End of current frame data. There should be no more bytes available.
      RCHECK(reader.bits_available() < 8);

      vpx_frame.is_keyframe = false;
      vpx_frame.uncompressed_header_size = vpx_frame.frame_size;
      vpx_frame.width = width_;
      vpx_frame.height = height_;
      continue;
    }

    bool is_interframe;
    RCHECK(reader.ReadBits(1, &is_interframe));
    vpx_frame.is_keyframe = !is_interframe;

    bool show_frame;
    RCHECK(reader.ReadBits(1, &show_frame));
    bool error_resilient_mode;
    RCHECK(reader.ReadBits(1, &error_resilient_mode));

    if (vpx_frame.is_keyframe) {
      RCHECK(ReadSyncCode(&reader));
      RCHECK(ReadBitDepthAndColorSpace(&reader, writable_codec_config()));
      RCHECK(ReadFrameSizes(&reader, &width_, &height_));
    } else {
      bool intra_only = false;
      if (!show_frame)
        RCHECK(reader.ReadBits(1, &intra_only));
      if (!error_resilient_mode)
        RCHECK(reader.SkipBits(2));  // reset_frame_context

      if (intra_only) {
        RCHECK(ReadSyncCode(&reader));
        if (codec_config().profile() > 0) {
          RCHECK(ReadBitDepthAndColorSpace(&reader, writable_codec_config()));
        } else {
          // NOTE: The intra-only frame header does not include the
          // specification of either the color format or color sub-sampling in
          // profile 0. VP9 specifies that the default color format should be
          // YUV 4:2:0 in this case (normative).
          writable_codec_config()->SetChromaSubsampling(
              VPCodecConfigurationRecord::CHROMA_420_COLLOCATED_WITH_LUMA);
          writable_codec_config()->set_bit_depth(8);
        }

        RCHECK(reader.SkipBits(REF_FRAMES));  // refresh_frame_flags
        RCHECK(ReadFrameSizes(&reader, &width_, &height_));
      } else {
        RCHECK(reader.SkipBits(REF_FRAMES));  // refresh_frame_flags
        RCHECK(reader.SkipBits(REFS_PER_FRAME * (REF_FRAMES_LOG2 + 1)));

        // TODO(kqyang): We may need to actually build the refs to extract the
        // correct width and height for the current frame. The width will be
        // used later in ReadTileInfo.
        RCHECK(ReadFrameSizesWithRefs(&reader, &width_, &height_));

        RCHECK(reader.SkipBits(1));  // allow_high_precision_mv

        bool interp_filter;
        RCHECK(reader.ReadBits(1, &interp_filter));
        if (!interp_filter)
          RCHECK(reader.SkipBits(2));  // more interp_filter
      }
    }

    if (!error_resilient_mode) {
      RCHECK(reader.SkipBits(1));  // refresh_frame_context
      RCHECK(reader.SkipBits(1));  // frame_parallel_decoding_mode
    }
    RCHECK(reader.SkipBits(FRAME_CONTEXTS_LOG2));  // frame_context_idx

    VLOG(4) << "bits read before ReadLoopFilter: " << reader.bit_position();
    RCHECK(ReadLoopFilter(&reader));
    RCHECK(ReadQuantization(&reader));
    RCHECK(ReadSegmentation(&reader));
    RCHECK(ReadTileInfo(width_, &reader));

    uint16_t header_size;
    RCHECK(reader.ReadBits(16, &header_size));
    vpx_frame.uncompressed_header_size =
        vpx_frame.frame_size - reader.bits_available() / 8;
    vpx_frame.width = width_;
    vpx_frame.height = height_;

    VLOG(3) << "\n frame_size: " << vpx_frame.frame_size
            << "\n uncompressed_header_size: "
            << vpx_frame.uncompressed_header_size
            << "\n bits read: " << reader.bit_position()
            << "\n header_size: " << header_size;

    RCHECK(header_size > 0);
    RCHECK(header_size * 8u <= reader.bits_available());

    data += vpx_frame.frame_size;
  }
  return true;
}

bool VP9Parser::IsKeyframe(const uint8_t* data, size_t data_size) {
  BitReader reader(data, data_size);
  uint8_t frame_marker;
  RCHECK(reader.ReadBits(2, &frame_marker));
  RCHECK(frame_marker == VP9_FRAME_MARKER);

  VPCodecConfigurationRecord codec_config;
  RCHECK(ReadProfile(&reader, &codec_config));

  bool show_existing_frame;
  RCHECK(reader.ReadBits(1, &show_existing_frame));
  if (show_existing_frame)
    return false;

  bool is_interframe;
  RCHECK(reader.ReadBits(1, &is_interframe));
  if (is_interframe)
    return false;

  RCHECK(reader.SkipBits(2));  // show_frame, error_resilient_mode.

  RCHECK(ReadSyncCode(&reader));
  return true;
}

}  // namespace media
}  // namespace shaka
