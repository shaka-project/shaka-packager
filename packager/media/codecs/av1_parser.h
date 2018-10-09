// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_AV1_PARSER_H_
#define PACKAGER_MEDIA_CODECS_AV1_PARSER_H_

#include <stdint.h>
#include <stdlib.h>

#include <vector>

namespace shaka {
namespace media {

class BitReader;

/// AV1 bitstream parser implemented according to av1 bitstream specification:
/// https://aomediacodec.github.io/av1-spec/.
class AV1Parser {
 public:
  struct Tile {
    size_t start_offset_in_bytes;
    size_t size_in_bytes;
  };

  AV1Parser();
  virtual ~AV1Parser();

  /// Parse an AV1 sample. Note that the sample data SHALL be a sequence of OBUs
  /// forming a Temporal Unit, with each OBU SHALL follow the
  /// open_bitstream_unit Low Overhead Bitstream Format syntax. See
  /// https://aomediacodec.github.io/av1-isobmff/#sampleformat for details.
  /// @param[out] on success, tiles will be filled with the tile information if
  ///             @a data contains Frame OBU or TileGroup OBU; It will be empty
  ///             otherwise.
  /// @return true on success, false otherwise.
  virtual bool Parse(const uint8_t* data,
                     size_t data_size,
                     std::vector<Tile>* tiles);

 private:
  AV1Parser(const AV1Parser&) = delete;
  AV1Parser& operator=(const AV1Parser&) = delete;

  // The structure names and the method names match the names in the spec but in
  // CamelCase.
  // Not all fields are populated. In particular, fields not referenced and not
  // needed to parse other parts of the bitstream are not populated.

  struct ObuExtensionHeader {
    int temporal_id = 0;
    int spatial_id = 0;
  };

  struct ObuHeader {
    int obu_type = 0;
    bool obu_has_size_field = false;
    ObuExtensionHeader extension_header;
  };

  struct ColorConfig {
    int bit_depth = 0;
    bool mono_chrome = false;
    int num_planes = 0;
    int color_primaries = 0;
    int transfer_chracteristics = 0;
    int matrix_coefficients = 0;
    bool color_range = false;
    bool subsampling_x = false;
    bool subsampling_y = false;
    int chroma_sampling_position = 0;
    bool separate_uv_delta_q = false;
  };

  struct TimingInfo {
    bool equal_picture_interval = false;
  };

  struct DecoderModelInfo {
    int buffer_delay_length_minus_1 = 0;
    int buffer_removal_time_length_minus_1 = 0;
    int frame_presentation_time_length_minus_1 = 0;
  };

  struct SequenceHeaderObu {
    int seq_profile = 0;
    bool reduced_still_picture_header = false;

    TimingInfo timing_info;
    bool decoder_model_info_present_flag = false;
    DecoderModelInfo decoder_model_info;

    int operating_points_cnt_minus_1 = 0;
    static constexpr int kMaxOperatingPointsCount = 1 << 5;
    int operating_point_idc[kMaxOperatingPointsCount] = {};
    bool decoder_model_present_for_this_op[kMaxOperatingPointsCount] = {};

    int frame_width_bits_minus_1 = 0;
    int frame_height_bits_minus_1 = 0;
    int max_frame_width_minus_1 = 0;
    int max_frame_height_minus_1 = 0;

    bool frame_id_numbers_present_flag = false;
    int delta_frame_id_length_minus_2 = 0;
    int additional_frame_id_length_minus_1 = 0;

    bool use_128x128_superblock = false;

    bool enable_warped_motion = false;
    bool enable_order_hint = false;
    bool enable_ref_frame_mvs = false;
    int order_hint_bits = 0;

    int seq_force_screen_content_tools = 0;
    int seq_force_integer_mv = 0;

    bool enable_superres = false;
    bool enable_cdef = false;
    bool enable_restoration = false;
    ColorConfig color_config;
    bool film_grain_params_present = false;
  };

  struct TileInfo {
    int tile_cols = 0;
    int tile_rows = 0;
    int tile_cols_log2 = 0;
    int tile_rows_log2 = 0;
    int tile_size_bytes = 0;
  };

  struct QuantizationParams {
    int base_q_idx = 0;
    int delta_qydc = 0;
    int delta_quac = 0;
    int delta_qudc = 0;
    int delta_qvac = 0;
    int delta_qvdc = 0;
  };

  static constexpr int kMaxSegments = 8;
  static constexpr int kSegLvlMax = 8;
  struct SegmentationParams {
    bool segmentation_enabled = false;
    bool feature_enabled[kMaxSegments][kSegLvlMax] = {};
    int feature_data[kMaxSegments][kSegLvlMax] = {};
  };

  static constexpr int kRefsPerFrame = 7;
  struct FrameHeaderObu {
    bool seen_frame_header = false;

    bool show_existing_frame = false;
    int frame_to_show_map_idx = 0;

    int frame_type = 0;
    int refresh_frame_flags = 0;

    int ref_frame_idx[kRefsPerFrame] = {};

    int order_hint = 0;

    int frame_width = 0;
    int frame_height = 0;
    int upscaled_width = 0;
    int render_width = 0;
    int render_height = 0;

    int mi_cols = 0;
    int mi_rows = 0;

    TileInfo tile_info;
    QuantizationParams quantization_params;
    SegmentationParams segmentation_params;
  };

  struct ReferenceFrame {
    int frame_type = 0;
    int order_hint = 0;

    int frame_width = 0;
    int frame_height = 0;
    int upscaled_width = 0;
    int render_width = 0;
    int render_height = 0;

    int mi_cols = 0;
    int mi_rows = 0;

    int bit_depth = 0;
    bool subsampling_x = false;
    bool subsampling_y = false;
  };

  bool ParseOpenBitstreamUnit(BitReader* reader, std::vector<Tile>* tiles);
  bool ParseObuHeader(BitReader* reader, ObuHeader* obu_header);
  bool ParseObuExtensionHeader(BitReader* reader,
                               ObuExtensionHeader* obu_extension_header);
  bool ParseTrailingBits(size_t nb_bits, BitReader* reader);
  bool ByteAlignment(BitReader* reader);

  // SequenceHeader OBU and children structures.
  bool ParseSequenceHeaderObu(BitReader* reader);
  bool ParseColorConfig(BitReader* reader);
  bool ParseTimingInfo(BitReader* reader);
  bool ParseDecoderModelInfo(BitReader* reader);
  bool SkipOperatingParametersInfo(BitReader* reader);

  // FrameHeader OBU and children structures.
  bool ParseFrameHeaderObu(const ObuHeader& obu_header, BitReader* reader);
  bool ParseUncompressedHeader(const ObuHeader& obu_header, BitReader* reader);
  int GetRelativeDist(int a, int b);
  bool ParseFrameSize(bool frame_size_override_flag, BitReader* reader);
  bool ParseRenderSize(BitReader* reader);
  bool ParseFrameSizeWithRefs(bool frame_size_override_flag, BitReader* reader);
  bool ParseSuperresParams(BitReader* reader);
  void ComputeImageSize();
  bool SkipInterpolationFilter(BitReader* reader);
  bool ParseLoopFilterParams(bool coded_lossless,
                             bool allow_intrabc,
                             BitReader* reader);
  bool ParseTileInfo(BitReader* reader);
  bool ParseQuantizationParams(BitReader* reader);
  bool ReadDeltaQ(BitReader* reader, int* delta_q);
  bool ParseSegmentationParams(int primary_ref_frame, BitReader* reader);
  bool SkipDeltaQParams(BitReader* reader, bool* delta_q_present);
  bool SkipDeltaLfParams(bool delta_q_present,
                         bool allow_intrabc,
                         BitReader* reader);
  bool ParseCdefParams(bool coded_lossless,
                       bool allow_intrabc,
                       BitReader* reader);
  bool ParseLrParams(bool all_lossless, bool allow_intrabc, BitReader* reader);
  bool SkipTxMode(bool coded_lossless, BitReader* reader);
  bool SkipSkipModeParams(bool frame_is_intra,
                          bool reference_select,
                          BitReader* reader);
  bool ParseFrameReferenceMode(bool frame_is_intra,
                               BitReader* reader,
                               bool* reference_select);
  bool SkipGlobalMotionParams(bool frame_is_intra,
                              bool allow_high_precision_mv,
                              BitReader* reader);
  bool SkipGlobalParam(int type,
                       int ref,
                       int idx,
                       bool allow_high_precision_mv,
                       BitReader* reader);
  bool SkipDecodeSignedSubexpWithRef(int low, int high, BitReader* reader);
  bool SkipDecodeUnsignedSubexpWithRef(int mx, BitReader* reader);
  bool SkipDecodeSubexp(int num_syms, BitReader* reader);
  bool SkipFilmGrainParams(bool show_frame,
                           bool showable_frame,
                           BitReader* reader);
  bool SkipTemporalPointInfo(BitReader* reader);

  // Frame OBU.
  bool ParseFrameObu(const ObuHeader& obu_header,
                     size_t size,
                     BitReader* reader,
                     std::vector<Tile>* tiles);

  // TileGroup OBU.
  bool ParseTileGroupObu(size_t size,
                         BitReader* reader,
                         std::vector<Tile>* tiles);
  bool SegFeatureActiveIdx(int idx, int feature);

  // Decoding process related helper functions.
  // We do not care about decoding itself, but we need to take care of reference
  // frame states.
  void DecodeFrameWrapup();
  bool SetFrameRefs(int last_frame_idx, int gold_frame_idx);
  int GetQIndex(bool ignore_delta_q, int segment_id);

  SequenceHeaderObu sequence_header_;
  FrameHeaderObu frame_header_;
  static constexpr int kNumRefFrames = 8;
  ReferenceFrame reference_frames_[kNumRefFrames];
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_AV1_PARSER_H_
