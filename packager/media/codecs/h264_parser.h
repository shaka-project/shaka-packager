// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H264 Annex-B video stream parser.

#ifndef PACKAGER_MEDIA_CODECS_H264_PARSER_H_
#define PACKAGER_MEDIA_CODECS_H264_PARSER_H_

#include <stdint.h>
#include <stdlib.h>

#include <map>
#include <memory>

#include "packager/media/codecs/h26x_bit_reader.h"
#include "packager/media/codecs/nalu_reader.h"

namespace shaka {
namespace media {

// On success, |coded_width| and |coded_height| contains coded resolution after
// cropping; |pixel_width:pixel_height| contains pixel aspect ratio, 1:1 is
// assigned if it is not present in SPS.
struct H264Sps;
bool ExtractResolutionFromSps(const H264Sps& sps,
                              uint32_t* coded_width,
                              uint32_t* coded_height,
                              uint32_t* pixel_width,
                              uint32_t* pixel_height);

enum {
  kH264ScalingList4x4Length = 16,
  kH264ScalingList8x8Length = 64,
};

struct H264Sps {
  int profile_idc;
  bool constraint_set0_flag;
  bool constraint_set1_flag;
  bool constraint_set2_flag;
  bool constraint_set3_flag;
  bool constraint_set4_flag;
  bool constraint_set5_flag;
  int level_idc;
  int seq_parameter_set_id;

  int chroma_format_idc;
  bool separate_colour_plane_flag;
  int bit_depth_luma_minus8;
  int bit_depth_chroma_minus8;
  bool qpprime_y_zero_transform_bypass_flag;

  bool seq_scaling_matrix_present_flag;
  int scaling_list4x4[6][kH264ScalingList4x4Length];
  int scaling_list8x8[6][kH264ScalingList8x8Length];

  int log2_max_frame_num_minus4;
  int pic_order_cnt_type;
  int log2_max_pic_order_cnt_lsb_minus4;
  bool delta_pic_order_always_zero_flag;
  int offset_for_non_ref_pic;
  int offset_for_top_to_bottom_field;
  int num_ref_frames_in_pic_order_cnt_cycle;
  int expected_delta_per_pic_order_cnt_cycle;  // calculated
  int offset_for_ref_frame[255];
  int max_num_ref_frames;
  bool gaps_in_frame_num_value_allowed_flag;
  int pic_width_in_mbs_minus1;
  int pic_height_in_map_units_minus1;
  bool frame_mbs_only_flag;
  bool mb_adaptive_frame_field_flag;
  bool direct_8x8_inference_flag;
  bool frame_cropping_flag;
  int frame_crop_left_offset;
  int frame_crop_right_offset;
  int frame_crop_top_offset;
  int frame_crop_bottom_offset;

  bool vui_parameters_present_flag;
  int sar_width;    // Set to 0 when not specified.
  int sar_height;   // Set to 0 when not specified.
  int transfer_characteristics;

  bool timing_info_present_flag;
  long num_units_in_tick;
  long time_scale;
  bool fixed_frame_rate_flag;

  bool bitstream_restriction_flag;
  int max_num_reorder_frames;
  int max_dec_frame_buffering;

  int chroma_array_type;
};

struct H264Pps {
  int pic_parameter_set_id;
  int seq_parameter_set_id;
  bool entropy_coding_mode_flag;
  bool bottom_field_pic_order_in_frame_present_flag;
  int num_slice_groups_minus1;
  int num_ref_idx_l0_default_active_minus1;
  int num_ref_idx_l1_default_active_minus1;
  bool weighted_pred_flag;
  int weighted_bipred_idc;
  int pic_init_qp_minus26;
  int pic_init_qs_minus26;
  int chroma_qp_index_offset;
  bool deblocking_filter_control_present_flag;
  bool constrained_intra_pred_flag;
  bool redundant_pic_cnt_present_flag;
  bool transform_8x8_mode_flag;

  bool pic_scaling_matrix_present_flag;
  int scaling_list4x4[6][kH264ScalingList4x4Length];
  int scaling_list8x8[6][kH264ScalingList8x8Length];

  int second_chroma_qp_index_offset;
};

struct H264ModificationOfPicNum {
  int modification_of_pic_nums_idc;
  union {
    int abs_diff_pic_num_minus1;
    int long_term_pic_num;
  };
};

struct H264WeightingFactors {
  bool luma_weight_flag[32];
  bool chroma_weight_flag[32];
  int luma_weight[32];
  int luma_offset[32];
  int chroma_weight[32][2];
  int chroma_offset[32][2];
};

struct H264DecRefPicMarking {
  int memory_mgmnt_control_operation;
  int difference_of_pic_nums_minus1;
  int long_term_pic_num;
  int long_term_frame_idx;
  int max_long_term_frame_idx_plus1;
};

struct H264SliceHeader {
  enum {
    kRefListSize = 32,
    kRefListModSize = kRefListSize
  };

  enum Type {
    kPSlice = 0,
    kBSlice = 1,
    kISlice = 2,
    kSPSlice = 3,
    kSISlice = 4,
  };

  bool IsPSlice() const;
  bool IsBSlice() const;
  bool IsISlice() const;
  bool IsSPSlice() const;
  bool IsSISlice() const;

  bool idr_pic_flag;       // from NAL header
  int nal_ref_idc;         // from NAL header
  // Points to the beginning of the nal unit.
  const uint8_t* nalu_data;

  // Size of whole nalu unit.
  size_t nalu_size;

  // This is the size of the slice header not including the nalu header byte.
  // Sturcture: |NALU Header|     Slice Header    |    Slice Data    |
  // Size:      |<- 8bits ->|<- header_bit_size ->|<- Rest of nalu ->|
  // Note that this is not a field in the H.264 spec.
  size_t header_bit_size;

  int first_mb_in_slice;
  int slice_type;
  int pic_parameter_set_id;
  int colour_plane_id;
  int frame_num;
  bool field_pic_flag;
  bool bottom_field_flag;
  int idr_pic_id;
  int pic_order_cnt_lsb;
  int delta_pic_order_cnt_bottom;
  int delta_pic_order_cnt[2];
  int redundant_pic_cnt;
  bool direct_spatial_mv_pred_flag;

  bool num_ref_idx_active_override_flag;
  int num_ref_idx_l0_active_minus1;
  int num_ref_idx_l1_active_minus1;
  bool ref_pic_list_modification_flag_l0;
  bool ref_pic_list_modification_flag_l1;
  H264ModificationOfPicNum ref_list_l0_modifications[kRefListModSize];
  H264ModificationOfPicNum ref_list_l1_modifications[kRefListModSize];

  int luma_log2_weight_denom;
  int chroma_log2_weight_denom;

  H264WeightingFactors pred_weight_table_l0;
  H264WeightingFactors pred_weight_table_l1;

  bool no_output_of_prior_pics_flag;
  bool long_term_reference_flag;

  bool adaptive_ref_pic_marking_mode_flag;
  H264DecRefPicMarking ref_pic_marking[kRefListSize];

  int cabac_init_idc;
  int slice_qp_delta;
  bool sp_for_switch_flag;
  int slice_qs_delta;
  int disable_deblocking_filter_idc;
  int slice_alpha_c0_offset_div2;
  int slice_beta_offset_div2;
};

struct H264SEIRecoveryPoint {
  int recovery_frame_cnt;
  bool exact_match_flag;
  bool broken_link_flag;
  int changing_slice_group_idc;
};

struct H264SEIMessage {
  enum Type {
    kSEIRecoveryPoint = 6,
  };

  int type;
  int payload_size;
  union {
    // Placeholder; in future more supported types will contribute to more
    // union members here.
    H264SEIRecoveryPoint recovery_point;
  };
};

// Class to parse an Annex-B H.264 stream,
// as specified in chapters 7 and Annex B of the H.264 spec.
class H264Parser {
 public:
  enum Result {
    kOk,
    kInvalidStream,      // error in stream
    kUnsupportedStream,  // stream not supported by the parser
    kEOStream,           // end of stream
  };

  H264Parser();
  ~H264Parser();

  // NALU-specific parsing functions.

  // SPSes and PPSes are owned by the parser class and the memory for their
  // structures is managed here, not by the caller, as they are reused
  // across NALUs.
  //
  // Parse an SPS/PPS NALU and save their data in the parser, returning id
  // of the parsed structure in |*pps_id|/|*sps_id|.
  // To get a pointer to a given SPS/PPS structure, use GetSps()/GetPps(),
  // passing the returned |*sps_id|/|*pps_id| as parameter.
  Result ParseSps(const Nalu& nalu, int* sps_id);
  Result ParsePps(const Nalu& nalu, int* pps_id);

  // Return a pointer to SPS/PPS with given |sps_id|/|pps_id| or NULL if not
  // present.
  const H264Sps* GetSps(int sps_id);
  const H264Pps* GetPps(int pps_id);

  // Slice headers and SEI messages are not used across NALUs by the parser
  // and can be discarded after current NALU, so the parser does not store
  // them, nor does it manage their memory.
  // The caller has to provide and manage it instead.

  // Parse a slice header, returning it in |*shdr|. |*nalu| must be set to
  // the NALU returned from AdvanceToNextNALU() and corresponding to |*shdr|.
  Result ParseSliceHeader(const Nalu& nalu, H264SliceHeader* shdr);

  // Parse a SEI message, returning it in |*sei_msg|, provided and managed
  // by the caller.
  Result ParseSEI(const Nalu& nalu, H264SEIMessage* sei_msg);

 private:
  // Parse scaling lists (see spec).
  Result ParseScalingList(H26xBitReader* br,
                          int size,
                          int* scaling_list,
                          bool* use_default);
  Result ParseSpsScalingLists(H26xBitReader* br, H264Sps* sps);
  Result ParsePpsScalingLists(H26xBitReader* br,
                              const H264Sps& sps,
                              H264Pps* pps);

  // Parse optional VUI parameters in SPS (see spec).
  Result ParseVUIParameters(H26xBitReader* br, H264Sps* sps);
  // Set |hrd_parameters_present| to true only if they are present.
  Result ParseAndIgnoreHRDParameters(H26xBitReader* br,
                                     bool* hrd_parameters_present);

  // Parse reference picture lists' modifications (see spec).
  Result ParseRefPicListModifications(H26xBitReader* br, H264SliceHeader* shdr);
  Result ParseRefPicListModification(H26xBitReader* br,
                                     int num_ref_idx_active_minus1,
                                     H264ModificationOfPicNum* ref_list_mods);

  // Parse prediction weight table (see spec).
  Result ParsePredWeightTable(H26xBitReader* br,
                              const H264Sps& sps,
                              H264SliceHeader* shdr);

  // Parse weighting factors (see spec).
  Result ParseWeightingFactors(H26xBitReader* br,
                               int num_ref_idx_active_minus1,
                               int chroma_array_type,
                               int luma_log2_weight_denom,
                               int chroma_log2_weight_denom,
                               H264WeightingFactors* w_facts);

  // Parse decoded reference picture marking information (see spec).
  Result ParseDecRefPicMarking(H26xBitReader* br, H264SliceHeader* shdr);

  // PPSes and SPSes stored for future reference.
  typedef std::map<int, std::unique_ptr<H264Sps>> SpsById;
  typedef std::map<int, std::unique_ptr<H264Pps>> PpsById;
  SpsById active_SPSes_;
  PpsById active_PPSes_;

  DISALLOW_COPY_AND_ASSIGN(H264Parser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_H264_PARSER_H_
