// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_H265_PARSER_H_
#define PACKAGER_MEDIA_CODECS_H265_PARSER_H_

#include <map>
#include <memory>
#include <vector>

#include "packager/media/codecs/h26x_bit_reader.h"

namespace shaka {
namespace media {

class Nalu;

enum H265SliceType { kBSlice = 0, kPSlice = 1, kISlice = 2 };

const int kMaxRefPicSetCount = 16;

// On success, |coded_width| and |coded_height| contains coded resolution after
// cropping; |pixel_width:pixel_height| contains pixel aspect ratio, 1:1 is
// assigned if it is not present in SPS.
struct H265Sps;
bool ExtractResolutionFromSps(const H265Sps& sps,
                              uint32_t* coded_width,
                              uint32_t* coded_height,
                              uint32_t* pixel_width,
                              uint32_t* pixel_height);

struct H265ReferencePictureSet {
  int delta_poc_s0[kMaxRefPicSetCount];
  int delta_poc_s1[kMaxRefPicSetCount];
  bool used_by_curr_pic_s0[kMaxRefPicSetCount];
  bool used_by_curr_pic_s1[kMaxRefPicSetCount];

  int num_negative_pics;
  int num_positive_pics;
  int num_delta_pocs;
};

struct H265VuiParameters {
  enum { kExtendedSar = 255 };

  bool aspect_ratio_info_present_flag = false;
  int aspect_ratio_idc = 0;
  int sar_width = 0;
  int sar_height = 0;
  int transfer_characteristics = 0;

  bool vui_timing_info_present_flag = false;
  long vui_num_units_in_tick = 0;
  long vui_time_scale = 0;

  bool bitstream_restriction_flag = false;
  int min_spatial_segmentation_idc = 0;

  // Incomplete...
};

struct H265Pps {
  H265Pps();
  ~H265Pps();

  // Many of the fields here are required when parsing so the default here may
  // not be valid.

  int pic_parameter_set_id = 0;
  int seq_parameter_set_id = 0;

  bool dependent_slice_segments_enabled_flag = false;
  bool output_flag_present_flag = false;
  int num_extra_slice_header_bits = 0;
  bool sign_data_hiding_enabled_flag = false;
  bool cabac_init_present_flag = false;

  int num_ref_idx_l0_default_active_minus1 = 0;
  int num_ref_idx_l1_default_active_minus1 = 0;
  int init_qp_minus26 = 0;
  bool constrained_intra_pred_flag = false;
  bool transform_skip_enabled_flag = false;

  bool cu_qp_delta_enabled_flag = 0;
  int diff_cu_qp_delta_depth = 0;
  int cb_qp_offset = 0;
  int cr_qp_offset = 0;

  bool slice_chroma_qp_offsets_present_flag = false;
  bool weighted_pred_flag = false;
  bool weighted_bipred_flag = false;
  bool transquant_bypass_enabled_flag = false;
  bool tiles_enabled_flag = false;
  bool entropy_coding_sync_enabled_flag = false;

  int num_tile_columns_minus1 = 0;
  int num_tile_rows_minus1 = 0;
  bool uniform_spacing_flag = true;
  std::vector<int> column_width_minus1;
  std::vector<int> row_height_minus1;
  bool loop_filter_across_tiles_enabled_flag = true;

  bool loop_filter_across_slices_enabled_flag = false;
  bool deblocking_filter_control_present_flag = false;
  bool deblocking_filter_override_enabled_flag = false;
  bool deblocking_filter_disabled_flag = false;
  int beta_offset_div2 = 0;
  int tc_offset_div2 = 0;

  bool scaling_list_data_present_flag = false;
  // Ignored: scaling_list_data( )

  bool lists_modification_present_flag = false;
  int log2_parallel_merge_level_minus2 = 0;
  bool slice_segment_header_extension_present_flag = false;

  // Incomplete: pps_range_extension:
  bool chroma_qp_offset_list_enabled_flag = false;

  // Ignored: extensions...
};

struct H265Sps {
  H265Sps();
  ~H265Sps();

  int GetPicSizeInCtbsY() const;
  int GetChromaArrayType() const;

  // Many of the fields here are required when parsing so the default here may
  // not be valid.

  int video_parameter_set_id = 0;
  int max_sub_layers_minus1 = 0;
  bool temporal_id_nesting_flag = false;

  // general_profile_space (2), general_tier_flag (1), general_profile_idc (5),
  // general_profile_compatibility_flags (32),
  // general_constraint_indicator_flags (48), general_level_idc (8).
  int general_profile_tier_level_data[12] = {};

  int seq_parameter_set_id = 0;

  int chroma_format_idc = 0;
  bool separate_colour_plane_flag = false;
  int pic_width_in_luma_samples = 0;
  int pic_height_in_luma_samples = 0;

  bool conformance_window_flag = false;
  int conf_win_left_offset = 0;
  int conf_win_right_offset = 0;
  int conf_win_top_offset = 0;
  int conf_win_bottom_offset = 0;

  int bit_depth_luma_minus8 = 0;
  int bit_depth_chroma_minus8 = 0;
  int log2_max_pic_order_cnt_lsb_minus4 = 0;

  bool sub_layer_ordering_info_present_flag = false;
  int max_dec_pic_buffering_minus1[8];
  int max_num_reorder_pics[8];
  int max_latency_increase_plus1[8];

  int log2_min_luma_coding_block_size_minus3 = 0;
  int log2_diff_max_min_luma_coding_block_size = 0;
  int log2_min_luma_transform_block_size_minus2 = 0;
  int log2_diff_max_min_luma_transform_block_size = 0;
  int max_transform_hierarchy_depth_inter = 0;
  int max_transform_hierarchy_depth_intra = 0;

  bool scaling_list_enabled_flag = false;
  bool scaling_list_data_present_flag = false;
  // Ignored: scaling_list_data()

  bool amp_enabled_flag = false;
  bool sample_adaptive_offset_enabled_flag = false;
  bool pcm_enabled_flag = false;
  int pcm_sample_bit_depth_luma_minus1 = 0;
  int pcm_sample_bit_depth_chroma_minus1 = 0;
  int log2_min_pcm_luma_coding_block_size_minus3 = 0;
  int log2_diff_max_min_pcm_luma_coding_block_size = 0;
  bool pcm_loop_filter_disabled_flag = false;

  int num_short_term_ref_pic_sets = 0;
  std::vector<H265ReferencePictureSet> st_ref_pic_sets;

  bool long_term_ref_pic_present_flag = false;
  int num_long_term_ref_pics = 0;
  std::vector<int> lt_ref_pic_poc_lsb;
  std::vector<bool> used_by_curr_pic_lt_flag;

  bool temporal_mvp_enabled_flag = false;
  bool strong_intra_smoothing_enabled_flag = false;

  bool vui_parameters_present = false;
  H265VuiParameters vui_parameters;

  // Ignored: extensions...
};

struct H265ReferencePictureListModifications {
  H265ReferencePictureListModifications();
  ~H265ReferencePictureListModifications();

  bool ref_pic_list_modification_flag_l0 = false;
  std::vector<int> list_entry_l0;

  bool ref_pic_list_modification_flag_l1 = false;
  std::vector<int> list_entry_l1;
};

struct H265SliceHeader {
  H265SliceHeader();
  ~H265SliceHeader();

  struct LongTermPicsInfo {
    bool delta_poc_msb_present_flag;
    int delta_poc_msb_cycle_lt;
  };
  // This is the value UsedByCurrPicLt for the current slice segment.  This
  // value is calulated from the LongTermPicsInfo during parsing.
  int used_by_curr_pic_lt = 0;

  // Many of the fields here are required when parsing so the default here may
  // not be valid.

  // This is the size of the slice header not including the nalu header byte.
  // Sturcture: |NALU Header |     Slice Header    |    Slice Data    |
  // Size:      |<- 16bits ->|<- header_bit_size ->|<- Rest of nalu ->|
  // Note that this is not a field in the H.265 spec.
  size_t header_bit_size = 0;

  bool first_slice_segment_in_pic_flag = false;
  bool no_output_of_prior_pics_flag = false;
  int pic_parameter_set_id = 0;

  bool dependent_slice_segment_flag = false;
  int segment_address = 0;
  int slice_type = 0;
  bool pic_output_flag = true;
  int colour_plane_id = 0;
  int slice_pic_order_cnt_lsb = 0;

  bool short_term_ref_pic_set_sps_flag = false;
  H265ReferencePictureSet st_ref_pic_set;
  int short_term_ref_pic_set_idx = 0;

  int num_long_term_sps = 0;
  int num_long_term_pics = 0;
  std::vector<LongTermPicsInfo> long_term_pics_info;

  bool slice_temporal_mvp_enabled_flag = false;
  bool slice_sao_luma_flag = false;
  bool slice_sao_chroma_flag = false;

  bool num_ref_idx_active_override_flag = false;
  int num_ref_idx_l0_active_minus1 = 0;
  int num_ref_idx_l1_active_minus1 = 0;

  H265ReferencePictureListModifications ref_pic_lists_modification;

  bool mvd_l1_zero_flag = false;
  bool cabac_init_flag = false;
  bool collocated_from_l0 = true;
  int collocated_ref_idx = 0;

  int five_minus_max_num_merge_cand = 0;
  int slice_qp_delta = 0;
  int slice_cb_qp_offset = 0;
  int slice_cr_qp_offset = 0;

  bool cu_chroma_qp_offset_enabled_flag = false;
  bool deblocking_filter_override_flag = false;
  bool slice_deblocking_filter_disabled_flag = false;
  int slice_beta_offset_div2 = 0;
  int slice_tc_offset_div2 = 0;
  bool slice_loop_filter_across_slices_enabled_flag = false;

  int num_entry_point_offsets = 0;
  int offset_len_minus1 = 0;
  std::vector<int> entry_point_offset_minus1;
};

/// A class to parse H.265 streams.  This is incomplete and skips many pieces.
/// This will mostly parse PPS and SPS elements as well as fully parse a
/// slice header.
class H265Parser {
 public:
  enum Result {
    kOk,
    kInvalidStream,      // error in stream
    kUnsupportedStream,  // stream not supported by the parser
    kEOStream,           // end of stream
  };

  H265Parser();
  ~H265Parser();

  /// Parses a video slice header.  If this returns kOk, then |*slice_header|
  /// will contain the parsed header; if it returns something else, the
  /// contents of |*slice_header| are undefined.
  Result ParseSliceHeader(const Nalu& nalu, H265SliceHeader* slice_header);

  /// Parses a PPS element.  This object is owned and managed by this class.
  /// The unique ID of the parsed PPS is stored in |*pps_id| if kOk is returned.
  Result ParsePps(const Nalu& nalu, int* pps_id);
  /// Parses a SPS element.  This object is owned and managed by this class.
  /// The unique ID of the parsed SPS is stored in |*sps_id| if kOk is returned.
  Result ParseSps(const Nalu& nalu, int* sps_id);

  /// @return a pointer to the PPS with the given ID, or NULL if none exists.
  const H265Pps* GetPps(int pps_id);
  /// @return a pointer to the SPS with the given ID, or NULL if none exists.
  const H265Sps* GetSps(int sps_id);

 private:
  Result ParseVuiParameters(int max_num_sub_layers_minus1,
                            H26xBitReader* br,
                            H265VuiParameters* vui);

  Result ParseReferencePictureSet(
      int num_short_term_ref_pic_sets,
      int st_rpx_idx,
      const std::vector<H265ReferencePictureSet>& ref_pic_sets,
      H26xBitReader* br,
      H265ReferencePictureSet* st_ref_pic_set);

  Result SkipReferencePictureListModification(
      const H265SliceHeader& slice_header,
      const H265Pps& pps,
      int num_pic_total_curr,
      H26xBitReader* br);

  Result SkipPredictionWeightTablePart(int num_ref_idx_minus1,
                                       int chroma_array_type,
                                       H26xBitReader* br);

  Result SkipPredictionWeightTable(bool is_b_slice,
                                   const H265Sps& sps,
                                   const H265SliceHeader& slice_header,
                                   H26xBitReader* br);

  Result ReadProfileTierLevel(bool profile_present,
                              int max_num_sub_layers_minus1,
                              H26xBitReader* br,
                              H265Sps* sps);

  Result SkipScalingListData(H26xBitReader* br);

  Result SkipHrdParameters(int max_num_sub_layers_minus1, H26xBitReader* br);

  Result SkipSubLayerHrdParameters(int cpb_cnt_minus1,
                                   bool sub_pic_hdr_params_present_flag,
                                   H26xBitReader* br);

  Result ByteAlignment(H26xBitReader* br);

  typedef std::map<int, std::unique_ptr<H265Sps>> SpsById;
  typedef std::map<int, std::unique_ptr<H265Pps>> PpsById;

  SpsById active_spses_;
  PpsById active_ppses_;

  DISALLOW_COPY_AND_ASSIGN(H265Parser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_H265_PARSER_H_
