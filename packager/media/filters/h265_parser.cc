// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/h265_parser.h"

#include <algorithm>
#include <math.h>

#include "packager/base/logging.h"
#include "packager/media/filters/nalu_reader.h"

#define TRUE_OR_RETURN(a)                            \
  do {                                               \
    if (!(a)) {                                      \
      DVLOG(1) << "Failure while processing " << #a; \
      return kInvalidStream;                         \
    }                                                \
  } while (0)

#define OK_OR_RETURN(a)  \
  do {                   \
    Result status = (a); \
    if (status != kOk)   \
      return status;     \
  } while (false)

namespace edash_packager {
namespace media {

namespace {
int GetNumPicTotalCurr(const H265SliceHeader& slice_header,
                       const H265Sps& sps) {
  int num_pic_total_curr = 0;
  const H265ReferencePictureSet& ref_pic_set =
      slice_header.short_term_ref_pic_set_sps_flag
          ? sps.st_ref_pic_sets[slice_header.short_term_ref_pic_set_idx]
          : slice_header.st_ref_pic_set;

  for (int i = 0; i < ref_pic_set.num_negative_pics; i++) {
    if (ref_pic_set.used_by_curr_pic_s0[i])
      num_pic_total_curr++;
  }
  for (int i = 0; i < ref_pic_set.num_positive_pics; i++) {
    if (ref_pic_set.used_by_curr_pic_s1[i])
      num_pic_total_curr++;
  }

  return num_pic_total_curr + slice_header.used_by_curr_pic_lt;
}
}  // namespace

H265Pps::H265Pps() {}
H265Pps::~H265Pps() {}

H265Sps::H265Sps() {}
H265Sps::~H265Sps() {}

int H265Sps::GetPicSizeInCtbsY() const {
  int min_cb_log2_size_y = log2_min_luma_coding_block_size_minus3 + 3;
  int ctb_log2_size_y =
      min_cb_log2_size_y + log2_diff_max_min_luma_coding_block_size;
  int ctb_size_y = 1 << ctb_log2_size_y;

  // Round-up division.
  int pic_width_in_ctbs_y = (pic_width_in_luma_samples - 1) / ctb_size_y + 1;
  int pic_height_in_ctbs_y = (pic_height_in_luma_samples - 1) / ctb_size_y + 1;
  return pic_width_in_ctbs_y * pic_height_in_ctbs_y;
}

int H265Sps::GetChromaArrayType() const {
  if (!separate_colour_plane_flag)
    return chroma_format_idc;
  else
    return 0;
}

H265ReferencePictureListModifications::H265ReferencePictureListModifications() {
}
H265ReferencePictureListModifications::
    ~H265ReferencePictureListModifications() {}

H265SliceHeader::H265SliceHeader() {}
H265SliceHeader::~H265SliceHeader() {}

H265Parser::H265Parser() {}
H265Parser::~H265Parser() {}

H265Parser::Result H265Parser::ParseSliceHeader(const Nalu& nalu,
                                                H265SliceHeader* slice_header) {
  DCHECK(nalu.is_video_slice());
  *slice_header = H265SliceHeader();

  // Parses whole element.
  H26xBitReader reader;
  reader.Initialize(nalu.data() + nalu.header_size(), nalu.payload_size());
  H26xBitReader* br = &reader;

  TRUE_OR_RETURN(br->ReadBool(&slice_header->first_slice_segment_in_pic_flag));
  if (nalu.type() >= Nalu::H265_BLA_W_LP &&
      nalu.type() <= Nalu::H265_RSV_IRAP_VCL23) {
    TRUE_OR_RETURN(br->ReadBool(&slice_header->no_output_of_prior_pics_flag));
  }

  TRUE_OR_RETURN(br->ReadUE(&slice_header->pic_parameter_set_id));
  const H265Pps* pps = GetPps(slice_header->pic_parameter_set_id);
  TRUE_OR_RETURN(pps);

  const H265Sps* sps = GetSps(pps->seq_parameter_set_id);
  TRUE_OR_RETURN(sps);

  if (!slice_header->first_slice_segment_in_pic_flag) {
    if (pps->dependent_slice_segments_enabled_flag) {
      TRUE_OR_RETURN(br->ReadBool(&slice_header->dependent_slice_segment_flag));
    }
    const int bit_length = ceil(log2(sps->GetPicSizeInCtbsY()));
    TRUE_OR_RETURN(br->ReadBits(bit_length, &slice_header->segment_address));
  }

  if (!slice_header->dependent_slice_segment_flag) {
    TRUE_OR_RETURN(br->SkipBits(pps->num_extra_slice_header_bits));
    TRUE_OR_RETURN(br->ReadUE(&slice_header->slice_type));
    if (pps->output_flag_present_flag) {
      TRUE_OR_RETURN(br->ReadBool(&slice_header->pic_output_flag));
    }
    if (sps->separate_colour_plane_flag) {
      TRUE_OR_RETURN(br->ReadBits(2, &slice_header->colour_plane_id));
    }

    if (nalu.type() != Nalu::H265_IDR_W_RADL &&
        nalu.type() != Nalu::H265_IDR_N_LP) {
      TRUE_OR_RETURN(br->ReadBits(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                                  &slice_header->slice_pic_order_cnt_lsb));

      TRUE_OR_RETURN(
          br->ReadBool(&slice_header->short_term_ref_pic_set_sps_flag));
      if (!slice_header->short_term_ref_pic_set_sps_flag) {
        OK_OR_RETURN(ParseReferencePictureSet(
            sps->num_short_term_ref_pic_sets, sps->num_short_term_ref_pic_sets,
            sps->st_ref_pic_sets, br, &slice_header->st_ref_pic_set));
      } else if (sps->num_short_term_ref_pic_sets > 1) {
        TRUE_OR_RETURN(
            br->ReadBits(ceil(log2(sps->num_short_term_ref_pic_sets)),
                         &slice_header->short_term_ref_pic_set_idx));
      }

      if (sps->long_term_ref_pic_present_flag) {
        if (sps->num_long_term_ref_pics > 0) {
          TRUE_OR_RETURN(br->ReadUE(&slice_header->num_long_term_sps));
        }
        TRUE_OR_RETURN(br->ReadUE(&slice_header->num_long_term_pics));

        const int pic_count =
            slice_header->num_long_term_sps + slice_header->num_long_term_pics;
        slice_header->long_term_pics_info.resize(pic_count);
        for (int i = 0; i < pic_count; i++) {
          if (i < slice_header->num_long_term_sps) {
            int lt_idx_sps = 0;
            if (sps->num_long_term_ref_pics > 1) {
              TRUE_OR_RETURN(br->ReadBits(
                  ceil(log2(sps->num_long_term_ref_pics)), &lt_idx_sps));
            }
            if (sps->used_by_curr_pic_lt_flag[lt_idx_sps])
              slice_header->used_by_curr_pic_lt++;
          } else {
            TRUE_OR_RETURN(br->SkipBits(sps->log2_max_pic_order_cnt_lsb_minus4 +
                                        4));  // poc_lsb_lt
            bool used_by_curr_pic_lt_flag;
            TRUE_OR_RETURN(br->ReadBool(&used_by_curr_pic_lt_flag));
            if (used_by_curr_pic_lt_flag)
              slice_header->used_by_curr_pic_lt++;
          }
          TRUE_OR_RETURN(br->ReadBool(&slice_header->long_term_pics_info[i]
                                           .delta_poc_msb_present_flag));
          if (slice_header->long_term_pics_info[i].delta_poc_msb_present_flag) {
            TRUE_OR_RETURN(br->ReadUE(
                &slice_header->long_term_pics_info[i].delta_poc_msb_cycle_lt));
          }
        }
      }

      if (sps->temporal_mvp_enabled_flag) {
        TRUE_OR_RETURN(
            br->ReadBool(&slice_header->slice_temporal_mvp_enabled_flag));
      }
    }

    if (nalu.nuh_layer_id() != 0) {
      NOTIMPLEMENTED() << "Multi-layer streams are not supported.";
      return kUnsupportedStream;
    }

    if (sps->sample_adaptive_offset_enabled_flag) {
      TRUE_OR_RETURN(br->ReadBool(&slice_header->slice_sao_luma_flag));
      if (sps->GetChromaArrayType() != 0) {
        TRUE_OR_RETURN(br->ReadBool(&slice_header->slice_sao_chroma_flag));
      }
    }

    slice_header->num_ref_idx_l0_active_minus1 =
        pps->num_ref_idx_l0_default_active_minus1;
    slice_header->num_ref_idx_l1_active_minus1 =
        pps->num_ref_idx_l1_default_active_minus1;
    if (slice_header->slice_type == kPSlice ||
        slice_header->slice_type == kBSlice) {
      TRUE_OR_RETURN(
          br->ReadBool(&slice_header->num_ref_idx_active_override_flag));
      if (slice_header->num_ref_idx_active_override_flag) {
        TRUE_OR_RETURN(br->ReadUE(&slice_header->num_ref_idx_l0_active_minus1));
        if (slice_header->slice_type == kBSlice) {
          TRUE_OR_RETURN(
              br->ReadUE(&slice_header->num_ref_idx_l1_active_minus1));
        }
      }

      const int num_pic_total_curr = GetNumPicTotalCurr(*slice_header, *sps);
      if (pps->lists_modification_present_flag && num_pic_total_curr > 1) {
        OK_OR_RETURN(SkipReferencePictureListModification(
            *slice_header, *pps, num_pic_total_curr, br));
      }

      if (slice_header->slice_type == kBSlice) {
        TRUE_OR_RETURN(br->ReadBool(&slice_header->mvd_l1_zero_flag));
      }
      if (pps->cabac_init_present_flag) {
        TRUE_OR_RETURN(br->ReadBool(&slice_header->cabac_init_flag));
      }
      if (slice_header->slice_temporal_mvp_enabled_flag) {
        if (slice_header->slice_type == kBSlice) {
          TRUE_OR_RETURN(br->ReadBool(&slice_header->collocated_from_l0));
        }
        bool l0_greater_than_0 = slice_header->num_ref_idx_l0_active_minus1 > 0;
        bool l1_greater_than_0 = slice_header->num_ref_idx_l1_active_minus1 > 0;
        if (slice_header->collocated_from_l0 ? l0_greater_than_0
                                             : l1_greater_than_0) {
          TRUE_OR_RETURN(br->ReadUE(&slice_header->collocated_ref_idx));
        }
      }

      if ((pps->weighted_pred_flag && slice_header->slice_type == kPSlice) ||
          (pps->weighted_bipred_flag && slice_header->slice_type == kBSlice)) {
        OK_OR_RETURN(SkipPredictionWeightTable(
            slice_header->slice_type == kBSlice, *sps, *slice_header, br));
      }
      TRUE_OR_RETURN(br->ReadUE(&slice_header->five_minus_max_num_merge_cand));
    }

    TRUE_OR_RETURN(br->ReadSE(&slice_header->slice_qp_delta));
    if (pps->slice_chroma_qp_offsets_present_flag) {
      TRUE_OR_RETURN(br->ReadSE(&slice_header->slice_cb_qp_offset));
      TRUE_OR_RETURN(br->ReadSE(&slice_header->slice_cr_qp_offset));
    }

    if (pps->chroma_qp_offset_list_enabled_flag) {
      TRUE_OR_RETURN(
          br->ReadBool(&slice_header->cu_chroma_qp_offset_enabled_flag));
    }
    if (pps->deblocking_filter_override_enabled_flag) {
      TRUE_OR_RETURN(
          br->ReadBool(&slice_header->deblocking_filter_override_flag));
    }
    if (slice_header->deblocking_filter_override_flag) {
      TRUE_OR_RETURN(
          br->ReadBool(&slice_header->slice_deblocking_filter_disabled_flag));
      if (!slice_header->slice_deblocking_filter_disabled_flag) {
        TRUE_OR_RETURN(br->ReadSE(&slice_header->slice_beta_offset_div2));
        TRUE_OR_RETURN(br->ReadSE(&slice_header->slice_tc_offset_div2));
      }
    }
    if (pps->loop_filter_across_slices_enabled_flag &&
        (slice_header->slice_sao_luma_flag ||
         slice_header->slice_sao_chroma_flag ||
         !slice_header->slice_deblocking_filter_disabled_flag)) {
      TRUE_OR_RETURN(br->ReadBool(
          &slice_header->slice_loop_filter_across_slices_enabled_flag));
    }
  }

  if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) {
    TRUE_OR_RETURN(br->ReadUE(&slice_header->num_entry_point_offsets));
    if (slice_header->num_entry_point_offsets > 0) {
      TRUE_OR_RETURN(br->ReadUE(&slice_header->offset_len_minus1));
      slice_header->entry_point_offset_minus1.resize(
          slice_header->num_entry_point_offsets);
      for (int i = 0; i < slice_header->num_entry_point_offsets; i++) {
        TRUE_OR_RETURN(
            br->ReadBits(slice_header->offset_len_minus1 + 1,
                         &slice_header->entry_point_offset_minus1[i]));
      }
    }
  }

  if (pps->slice_segment_header_extension_present_flag) {
    int extension_length;
    TRUE_OR_RETURN(br->ReadUE(&extension_length));
    TRUE_OR_RETURN(br->SkipBits(extension_length * 8));
  }

  size_t epb = br->NumEmulationPreventionBytesRead();
  slice_header->header_bit_size =
      (nalu.payload_size() - epb) * 8 - br->NumBitsLeft();

  return kOk;
}

H265Parser::Result H265Parser::ParsePps(const Nalu& nalu, int* pps_id) {
  DCHECK_EQ(Nalu::H265_PPS, nalu.type());

  // Reads most of the element, not reading the extension data.
  H26xBitReader reader;
  reader.Initialize(nalu.data() + nalu.header_size(), nalu.payload_size());
  H26xBitReader* br = &reader;

  *pps_id = -1;
  scoped_ptr<H265Pps> pps(new H265Pps);

  TRUE_OR_RETURN(br->ReadUE(&pps->pic_parameter_set_id));
  TRUE_OR_RETURN(br->ReadUE(&pps->seq_parameter_set_id));

  TRUE_OR_RETURN(br->ReadBool(&pps->dependent_slice_segments_enabled_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->output_flag_present_flag));
  TRUE_OR_RETURN(br->ReadBits(3, &pps->num_extra_slice_header_bits));
  TRUE_OR_RETURN(br->ReadBool(&pps->sign_data_hiding_enabled_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->cabac_init_present_flag));

  TRUE_OR_RETURN(br->ReadUE(&pps->num_ref_idx_l0_default_active_minus1));
  TRUE_OR_RETURN(br->ReadUE(&pps->num_ref_idx_l1_default_active_minus1));
  TRUE_OR_RETURN(br->ReadSE(&pps->init_qp_minus26));
  TRUE_OR_RETURN(br->ReadBool(&pps->constrained_intra_pred_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->transform_skip_enabled_flag));

  TRUE_OR_RETURN(br->ReadBool(&pps->cu_qp_delta_enabled_flag));
  if (pps->cu_qp_delta_enabled_flag)
    TRUE_OR_RETURN(br->ReadUE(&pps->diff_cu_qp_delta_depth));
  TRUE_OR_RETURN(br->ReadSE(&pps->cb_qp_offset));
  TRUE_OR_RETURN(br->ReadSE(&pps->cr_qp_offset));

  TRUE_OR_RETURN(br->ReadBool(&pps->slice_chroma_qp_offsets_present_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->weighted_pred_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->weighted_bipred_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->transquant_bypass_enabled_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->tiles_enabled_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->entropy_coding_sync_enabled_flag));

  if (pps->tiles_enabled_flag) {
    TRUE_OR_RETURN(br->ReadUE(&pps->num_tile_columns_minus1));
    TRUE_OR_RETURN(br->ReadUE(&pps->num_tile_rows_minus1));
    TRUE_OR_RETURN(br->ReadBool(&pps->uniform_spacing_flag));
    if (!pps->uniform_spacing_flag) {
      pps->column_width_minus1.resize(pps->num_tile_columns_minus1);
      for (int i = 0; i < pps->num_tile_columns_minus1; i++) {
        TRUE_OR_RETURN(br->ReadUE(&pps->column_width_minus1[i]));
      }
      pps->row_height_minus1.resize(pps->num_tile_rows_minus1);
      for (int i = 0; i < pps->num_tile_rows_minus1; i++) {
        TRUE_OR_RETURN(br->ReadUE(&pps->row_height_minus1[i]));
      }
    }
    TRUE_OR_RETURN(br->ReadBool(&pps->loop_filter_across_tiles_enabled_flag));
  }

  TRUE_OR_RETURN(br->ReadBool(&pps->loop_filter_across_slices_enabled_flag));
  TRUE_OR_RETURN(br->ReadBool(&pps->deblocking_filter_control_present_flag));
  if (pps->deblocking_filter_control_present_flag) {
    TRUE_OR_RETURN(br->ReadBool(&pps->deblocking_filter_override_enabled_flag));
    TRUE_OR_RETURN(br->ReadBool(&pps->deblocking_filter_disabled_flag));
    if (!pps->deblocking_filter_disabled_flag) {
      TRUE_OR_RETURN(br->ReadSE(&pps->beta_offset_div2));
      TRUE_OR_RETURN(br->ReadSE(&pps->tc_offset_div2));
    }
  }

  TRUE_OR_RETURN(br->ReadBool(&pps->scaling_list_data_present_flag));
  if (pps->scaling_list_data_present_flag) {
    OK_OR_RETURN(SkipScalingListData(br));
  }

  TRUE_OR_RETURN(br->ReadBool(&pps->lists_modification_present_flag));
  TRUE_OR_RETURN(br->ReadUE(&pps->log2_parallel_merge_level_minus2));

  TRUE_OR_RETURN(
      br->ReadBool(&pps->slice_segment_header_extension_present_flag));

  bool pps_extension_present_flag;
  bool pps_range_extension_flag = false;
  TRUE_OR_RETURN(br->ReadBool(&pps_extension_present_flag));
  if (pps_extension_present_flag) {
    TRUE_OR_RETURN(br->ReadBool(&pps_range_extension_flag));
    // pps_multilayer_extension_flag, pps_3d_extension_flag, pps_extension_5bits
    TRUE_OR_RETURN(br->SkipBits(1 + 1 + 5));
  }

  if (pps_range_extension_flag) {
    if (pps->transform_skip_enabled_flag) {
      // log2_max_transform_skip_block_size_minus2
      int ignored;
      TRUE_OR_RETURN(br->ReadUE(&ignored));
    }

    TRUE_OR_RETURN(br->SkipBits(1));  // cross_component_prediction_enabled_flag
    TRUE_OR_RETURN(br->ReadBool(&pps->chroma_qp_offset_list_enabled_flag));
    // Incomplete
  }

  // Ignore remaining extension data.

  // This will replace any existing PPS instance.  The scoped_ptr will delete
  // the memory when it is overwritten.
  *pps_id = pps->pic_parameter_set_id;
  active_ppses_[*pps_id] = pps.Pass();

  return kOk;
}

H265Parser::Result H265Parser::ParseSps(const Nalu& nalu, int* sps_id) {
  DCHECK_EQ(Nalu::H265_SPS, nalu.type());

  // Reads most of the element, not reading the extension data.
  H26xBitReader reader;
  reader.Initialize(nalu.data() + nalu.header_size(), nalu.payload_size());
  H26xBitReader* br = &reader;

  *sps_id = -1;

  scoped_ptr<H265Sps> sps(new H265Sps);

  TRUE_OR_RETURN(br->ReadBits(4, &sps->video_parameter_set_id));
  TRUE_OR_RETURN(br->ReadBits(3, &sps->max_sub_layers_minus1));
  TRUE_OR_RETURN(br->ReadBool(&sps->temporal_id_nesting_flag));

  OK_OR_RETURN(SkipProfileTierLevel(true, sps->max_sub_layers_minus1, br));

  TRUE_OR_RETURN(br->ReadUE(&sps->seq_parameter_set_id));
  TRUE_OR_RETURN(br->ReadUE(&sps->chroma_format_idc));
  if (sps->chroma_format_idc == 3) {
    TRUE_OR_RETURN(br->ReadBool(&sps->separate_colour_plane_flag));
  }
  TRUE_OR_RETURN(br->ReadUE(&sps->pic_width_in_luma_samples));
  TRUE_OR_RETURN(br->ReadUE(&sps->pic_height_in_luma_samples));

  TRUE_OR_RETURN(br->ReadBool(&sps->conformance_window_flag));
  if (sps->conformance_window_flag) {
    TRUE_OR_RETURN(br->ReadUE(&sps->conf_win_left_offset));
    TRUE_OR_RETURN(br->ReadUE(&sps->conf_win_right_offset));
    TRUE_OR_RETURN(br->ReadUE(&sps->conf_win_top_offset));
    TRUE_OR_RETURN(br->ReadUE(&sps->conf_win_bottom_offset));
  }

  TRUE_OR_RETURN(br->ReadUE(&sps->bit_depth_luma_minus8));
  TRUE_OR_RETURN(br->ReadUE(&sps->bit_depth_chroma_minus8));
  TRUE_OR_RETURN(br->ReadUE(&sps->log2_max_pic_order_cnt_lsb_minus4));

  TRUE_OR_RETURN(br->ReadBool(&sps->sub_layer_ordering_info_present_flag));
  int start = sps->sub_layer_ordering_info_present_flag
                  ? 0
                  : sps->max_sub_layers_minus1;
  for (int i = start; i <= sps->max_sub_layers_minus1; i++) {
    TRUE_OR_RETURN(br->ReadUE(&sps->max_dec_pic_buffering_minus1[i]));
    TRUE_OR_RETURN(br->ReadUE(&sps->max_num_reorder_pics[i]));
    TRUE_OR_RETURN(br->ReadUE(&sps->max_latency_increase_plus1[i]));
  }

  TRUE_OR_RETURN(br->ReadUE(&sps->log2_min_luma_coding_block_size_minus3));
  TRUE_OR_RETURN(br->ReadUE(&sps->log2_diff_max_min_luma_coding_block_size));
  TRUE_OR_RETURN(br->ReadUE(&sps->log2_min_luma_transform_block_size_minus2));
  TRUE_OR_RETURN(br->ReadUE(&sps->log2_diff_max_min_luma_transform_block_size));
  TRUE_OR_RETURN(br->ReadUE(&sps->max_transform_hierarchy_depth_inter));
  TRUE_OR_RETURN(br->ReadUE(&sps->max_transform_hierarchy_depth_intra));

  TRUE_OR_RETURN(br->ReadBool(&sps->scaling_list_enabled_flag));
  if (sps->scaling_list_enabled_flag) {
    TRUE_OR_RETURN(br->ReadBool(&sps->scaling_list_data_present_flag));
    if (sps->scaling_list_data_present_flag) {
      OK_OR_RETURN(SkipScalingListData(br));
    }
  }

  TRUE_OR_RETURN(br->ReadBool(&sps->amp_enabled_flag));
  TRUE_OR_RETURN(br->ReadBool(&sps->sample_adaptive_offset_enabled_flag));
  TRUE_OR_RETURN(br->ReadBool(&sps->pcm_enabled_flag));
  if (sps->pcm_enabled_flag) {
    TRUE_OR_RETURN(br->ReadBits(4, &sps->pcm_sample_bit_depth_luma_minus1));
    TRUE_OR_RETURN(br->ReadBits(4, &sps->pcm_sample_bit_depth_chroma_minus1));
    TRUE_OR_RETURN(
        br->ReadUE(&sps->log2_min_pcm_luma_coding_block_size_minus3));
    TRUE_OR_RETURN(
        br->ReadUE(&sps->log2_diff_max_min_pcm_luma_coding_block_size));
    TRUE_OR_RETURN(br->ReadBool(&sps->pcm_loop_filter_disabled_flag));
  }

  TRUE_OR_RETURN(br->ReadUE(&sps->num_short_term_ref_pic_sets));
  sps->st_ref_pic_sets.resize(sps->num_short_term_ref_pic_sets);
  for (int i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
    OK_OR_RETURN(ParseReferencePictureSet(sps->num_short_term_ref_pic_sets, i,
                                          sps->st_ref_pic_sets, br,
                                          &sps->st_ref_pic_sets[i]));
  }

  TRUE_OR_RETURN(br->ReadBool(&sps->long_term_ref_pic_present_flag));
  if (sps->long_term_ref_pic_present_flag) {
    TRUE_OR_RETURN(br->ReadUE(&sps->num_long_term_ref_pics));
    sps->lt_ref_pic_poc_lsb.resize(sps->num_long_term_ref_pics);
    sps->used_by_curr_pic_lt_flag.resize(sps->num_long_term_ref_pics);
    for (int i = 0; i < sps->num_long_term_ref_pics; i++) {
      TRUE_OR_RETURN(br->ReadBits(sps->log2_max_pic_order_cnt_lsb_minus4 + 4,
                                  &sps->lt_ref_pic_poc_lsb[i]));
      bool temp;
      TRUE_OR_RETURN(br->ReadBool(&temp));
      sps->used_by_curr_pic_lt_flag[i] = temp;
    }
  }

  TRUE_OR_RETURN(br->ReadBool(&sps->temporal_mvp_enabled_flag));
  TRUE_OR_RETURN(br->ReadBool(&sps->strong_intra_smoothing_enabled_flag));

  // Ignore remaining extension data.

  // This will replace any existing SPS instance.  The scoped_ptr will delete
  // the memory when it is overwritten.
  *sps_id = sps->seq_parameter_set_id;
  active_spses_[*sps_id] = sps.Pass();

  return kOk;
}

const H265Pps* H265Parser::GetPps(int pps_id) {
  return active_ppses_[pps_id].get();
}

const H265Sps* H265Parser::GetSps(int sps_id) {
  return active_spses_[sps_id].get();
}

H265Parser::Result H265Parser::ParseReferencePictureSet(
    int num_short_term_ref_pic_sets,
    int st_rps_idx,
    const std::vector<H265ReferencePictureSet>& ref_pic_sets,
    H26xBitReader* br,
    H265ReferencePictureSet* out_ref_pic_set) {
  // Parses and processess a short-term reference picture set.  This needs to
  // be done since the size of this element may be dependent on previous
  // reference picture sets.

  bool inter_ref_pic_set_prediction = false;
  if (st_rps_idx != 0) {
    TRUE_OR_RETURN(br->ReadBool(&inter_ref_pic_set_prediction));
  }

  if (inter_ref_pic_set_prediction) {
    int delta_idx = 1;
    if (st_rps_idx == num_short_term_ref_pic_sets) {
      TRUE_OR_RETURN(br->ReadUE(&delta_idx));
      delta_idx++;
      TRUE_OR_RETURN(delta_idx <= st_rps_idx);
    }

    int ref_rps_idx = st_rps_idx - delta_idx;
    DCHECK_LE(0, ref_rps_idx);
    DCHECK_LT(ref_rps_idx, st_rps_idx);

    bool delta_rps_sign;
    int abs_delta_rps_minus1;
    TRUE_OR_RETURN(br->ReadBool(&delta_rps_sign));
    TRUE_OR_RETURN(br->ReadUE(&abs_delta_rps_minus1));
    int delta_rps =
        delta_rps_sign ? -(abs_delta_rps_minus1 + 1) : abs_delta_rps_minus1 + 1;

    int ref_num_delta_pocs = ref_pic_sets[ref_rps_idx].num_delta_pocs;
    std::vector<bool> used_by_curr_pic(ref_num_delta_pocs + 1);
    std::vector<bool> use_delta(ref_num_delta_pocs + 1);
    for (int j = 0; j <= ref_num_delta_pocs; j++) {
      bool temp;
      TRUE_OR_RETURN(br->ReadBool(&temp));
      used_by_curr_pic[j] = temp;

      if (!used_by_curr_pic[j]) {
        TRUE_OR_RETURN(br->ReadBool(&temp));
        use_delta[j] = temp;
      } else {
        use_delta[j] = true;
      }
    }

    int ref_num_positive_pics = ref_pic_sets[ref_rps_idx].num_positive_pics;
    int ref_num_negative_pics = ref_pic_sets[ref_rps_idx].num_negative_pics;
    int i;

    // Update list 0.
    {
      i = 0;
      for (int j = ref_num_positive_pics - 1; j >= 0; j--) {
        int d_poc = ref_pic_sets[ref_rps_idx].delta_poc_s1[j] + delta_rps;
        if (d_poc < 0 && use_delta[ref_num_negative_pics + j]) {
          out_ref_pic_set->delta_poc_s0[i] = d_poc;
          out_ref_pic_set->used_by_curr_pic_s0[i] =
              used_by_curr_pic[ref_num_negative_pics + j];
          i++;
        }
      }
      if (delta_rps < 0 && use_delta[ref_num_delta_pocs]) {
        out_ref_pic_set->delta_poc_s0[i] = delta_rps;
        out_ref_pic_set->used_by_curr_pic_s0[i] =
            used_by_curr_pic[ref_num_delta_pocs];
        i++;
      }
      for (int j = 0; j < ref_num_negative_pics; j++) {
        int d_poc = ref_pic_sets[ref_rps_idx].delta_poc_s0[j] + delta_rps;
        if (d_poc < 0 && use_delta[j]) {
          out_ref_pic_set->delta_poc_s0[i] = d_poc;
          out_ref_pic_set->used_by_curr_pic_s0[i] = used_by_curr_pic[j];
          i++;
        }
      }
      out_ref_pic_set->num_negative_pics = i;
    }

    // Update list 1.
    {
      i = 0;
      for (int j = ref_num_negative_pics - 1; j >= 0; j--) {
        int d_poc = ref_pic_sets[ref_rps_idx].delta_poc_s0[j] + delta_rps;
        if (d_poc > 0 && use_delta[j]) {
          out_ref_pic_set->delta_poc_s1[i] = d_poc;
          out_ref_pic_set->used_by_curr_pic_s1[i] = used_by_curr_pic[j];
          i++;
        }
      }
      if (delta_rps > 0 && use_delta[ref_num_delta_pocs]) {
        out_ref_pic_set->delta_poc_s1[i] = delta_rps;
        out_ref_pic_set->used_by_curr_pic_s1[i] =
            used_by_curr_pic[ref_num_delta_pocs];
        i++;
      }
      for (int j = 0; j < ref_num_positive_pics; j++) {
        int d_poc = ref_pic_sets[ref_rps_idx].delta_poc_s1[j] + delta_rps;
        if (d_poc > 0 && use_delta[ref_num_negative_pics + j]) {
          out_ref_pic_set->delta_poc_s1[i] = d_poc;
          out_ref_pic_set->used_by_curr_pic_s1[i] =
              used_by_curr_pic[ref_num_negative_pics + j];
          i++;
        }
      }
      out_ref_pic_set->num_positive_pics = i;
    }
  } else {
    TRUE_OR_RETURN(br->ReadUE(&out_ref_pic_set->num_negative_pics));
    TRUE_OR_RETURN(br->ReadUE(&out_ref_pic_set->num_positive_pics));

    int prev_poc = 0;
    for (int i = 0; i < out_ref_pic_set->num_negative_pics; i++) {
      int delta_poc_s0_minus1;
      TRUE_OR_RETURN(br->ReadUE(&delta_poc_s0_minus1));
      out_ref_pic_set->delta_poc_s0[i] = prev_poc - (delta_poc_s0_minus1 + 1);
      prev_poc = out_ref_pic_set->delta_poc_s0[i];

      TRUE_OR_RETURN(br->ReadBool(&out_ref_pic_set->used_by_curr_pic_s0[i]));
    }

    prev_poc = 0;
    for (int i = 0; i < out_ref_pic_set->num_positive_pics; i++) {
      int delta_poc_s1_minus1;
      TRUE_OR_RETURN(br->ReadUE(&delta_poc_s1_minus1));
      out_ref_pic_set->delta_poc_s1[i] = prev_poc + delta_poc_s1_minus1 + 1;
      prev_poc = out_ref_pic_set->delta_poc_s1[i];

      TRUE_OR_RETURN(br->ReadBool(&out_ref_pic_set->used_by_curr_pic_s1[i]));
    }
  }

  out_ref_pic_set->num_delta_pocs =
      out_ref_pic_set->num_positive_pics + out_ref_pic_set->num_negative_pics;
  return kOk;
}

H265Parser::Result H265Parser::SkipReferencePictureListModification(
    const H265SliceHeader& slice_header,
    const H265Pps& pps,
    int num_pic_total_curr,
    H26xBitReader* br) {
  // Reads whole element but ignores it all.

  bool ref_pic_list_modification_flag_l0;
  TRUE_OR_RETURN(br->ReadBool(&ref_pic_list_modification_flag_l0));
  if (ref_pic_list_modification_flag_l0) {
    for (int i = 0; i <= pps.num_ref_idx_l0_default_active_minus1; i++) {
      TRUE_OR_RETURN(br->SkipBits(ceil(log2(num_pic_total_curr))));
    }
  }

  if (slice_header.slice_type == kBSlice) {
    bool ref_pic_list_modification_flag_l1;
    TRUE_OR_RETURN(br->ReadBool(&ref_pic_list_modification_flag_l1));
    if (ref_pic_list_modification_flag_l1) {
      for (int i = 0; i <= pps.num_ref_idx_l1_default_active_minus1; i++) {
        TRUE_OR_RETURN(br->SkipBits(ceil(log2(num_pic_total_curr))));
      }
    }
  }

  return kOk;
}

H265Parser::Result H265Parser::SkipPredictionWeightTablePart(
    int num_ref_idx_minus1,
    int chroma_array_type,
    H26xBitReader* br) {
  // Reads whole element, ignores it.
  int ignored;
  std::vector<bool> luma_weight_flag(num_ref_idx_minus1 + 1);
  std::vector<bool> chroma_weight_flag(num_ref_idx_minus1 + 1);

  for (int i = 0; i <= num_ref_idx_minus1; i++) {
    bool temp;
    TRUE_OR_RETURN(br->ReadBool(&temp));
    luma_weight_flag[i] = temp;
  }
  if (chroma_array_type != 0) {
    for (int i = 0; i <= num_ref_idx_minus1; i++) {
      bool temp;
      TRUE_OR_RETURN(br->ReadBool(&temp));
      chroma_weight_flag[i] = temp;
    }
  }
  for (int i = 0; i <= num_ref_idx_minus1; i++) {
    if (luma_weight_flag[i]) {
      TRUE_OR_RETURN(br->ReadSE(&ignored));  // delta_luma_weight_l#
      TRUE_OR_RETURN(br->ReadSE(&ignored));  // luma_offset_l#
    }
    if (chroma_weight_flag[i]) {
      for (int j = 0; j < 2; j++) {
        TRUE_OR_RETURN(br->ReadSE(&ignored));  // delta_chroma_weight_l#
        TRUE_OR_RETURN(br->ReadSE(&ignored));  // delta_chroma_offset_l#
      }
    }
  }

  return kOk;
}

H265Parser::Result H265Parser::SkipPredictionWeightTable(
    bool is_b_slice,
    const H265Sps& sps,
    const H265SliceHeader& slice_header,
    H26xBitReader* br) {
  // Reads whole element, ignores it.
  int ignored;
  int chroma_array_type = sps.GetChromaArrayType();

  TRUE_OR_RETURN(br->ReadUE(&ignored));  // luma_log2_weight_denom
  if (chroma_array_type != 0) {
    TRUE_OR_RETURN(br->ReadSE(&ignored));  // delta_chroma_log2_weight_denom
  }
  OK_OR_RETURN(SkipPredictionWeightTablePart(
      slice_header.num_ref_idx_l0_active_minus1, chroma_array_type, br));
  if (is_b_slice) {
    OK_OR_RETURN(SkipPredictionWeightTablePart(
        slice_header.num_ref_idx_l1_active_minus1, chroma_array_type, br));
  }

  return kOk;
}

H265Parser::Result H265Parser::SkipProfileTierLevel(
    bool profile_present,
    int max_num_sub_layers_minus1,
    H26xBitReader* br) {
  // Reads whole element, ignores it.

  if (profile_present) {
    // general_profile_space, general_tier_flag, general_profile_idc
    // general_profile_compativility_flag
    // general_progressive_source_flag
    // general_interlaced_source_flag
    // general_non_packed_constraint_flag
    // general_frame_only_constraint_flag
    // 44-bits of other flags
    TRUE_OR_RETURN(br->SkipBits(2 + 1 + 5 + 32 + 4 + 44));
  }

  TRUE_OR_RETURN(br->SkipBits(8));  // general_level_idc

  std::vector<bool> sub_layer_profile_present(max_num_sub_layers_minus1);
  std::vector<bool> sub_layer_level_present(max_num_sub_layers_minus1);
  for (int i = 0; i < max_num_sub_layers_minus1; i++) {
    bool profile, level;
    TRUE_OR_RETURN(br->ReadBool(&profile));
    TRUE_OR_RETURN(br->ReadBool(&level));
    sub_layer_profile_present[i] = profile;
    sub_layer_level_present[i] = level;
  }

  if (max_num_sub_layers_minus1 > 0) {
    for (int i = max_num_sub_layers_minus1; i < 8; i++)
      TRUE_OR_RETURN(br->SkipBits(2));  // reserved_zero_2bits
  }

  for (int i = 0; i < max_num_sub_layers_minus1; i++) {
    if (sub_layer_profile_present[i]) {
      // sub_layer_profile_space, sub_layer_tier_flag, sub_layer_profile_idc
      // sub_layer_profile_compatibility
      // sub_layer_reserved_zero_43bits
      // sub_layer_reserved_zero_bit
      TRUE_OR_RETURN(br->SkipBits(2 + 1 + 5 + 32 + 4 + 43 + 1));
    }
    if (sub_layer_level_present[i]) {
      TRUE_OR_RETURN(br->SkipBits(8));
    }
  }

  return kOk;
}

H265Parser::Result H265Parser::SkipScalingListData(H26xBitReader* br) {
  // Reads whole element, ignores it.
  int ignored;
  for (int size_id = 0; size_id < 4; size_id++) {
    for (int matrix_id = 0; matrix_id < 6;
         matrix_id += ((size_id == 3) ? 3 : 1)) {
      bool scaling_list_pred_mode;
      TRUE_OR_RETURN(br->ReadBool(&scaling_list_pred_mode));
      if (!scaling_list_pred_mode) {
        // scaling_list_pred_matrix_id_delta
        TRUE_OR_RETURN(br->ReadUE(&ignored));
      } else {
        int coefNum = std::min(64, (1 << (4 + (size_id << 1))));
        if (size_id > 1) {
          TRUE_OR_RETURN(br->ReadSE(&ignored));  // scaling_list_dc_coef_minus8
        }

        for (int i = 0; i < coefNum; i++) {
          TRUE_OR_RETURN(br->ReadSE(&ignored));  // scaling_list_delta_coef
        }
      }
    }
  }

  return kOk;
}

}  // namespace media
}  // namespace edash_packager
