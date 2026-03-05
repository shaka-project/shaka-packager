// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_AC4_PARSER_H_
#define PACKAGER_MEDIA_CODECS_AC4_PARSER_H_

#include <cstdint>
#include <cstdlib>
#include <vector>

namespace shaka {
namespace media {

class BitReader;

class AC4Parser {
 public:
  struct FrameRateMultiplyInfo {
    int b_multiplier = 0;
    int multiplier_bit = 0;
    int dsi_frame_rate_multiply_info = 0;
  };

  struct FrameRateFractionsInfo {
    int b_frame_rate_fraction = 0;
    int b_frame_rate_fraction_is_4 = 0;
  };

  struct EmdfInfo {
    int emdf_version = 0;
    int key_id = 0;
    int b_emdf_payloads_substream_info = 0;
    int substream_index = 0;
    // indent for emdf_protection
    int protection_length_primary = 0;
    int protection_length_secondary = 0;
    uint8_t protection_bits_primary[16];
    uint8_t protection_bits_secondary[16];
  };

  struct OamdCommonData {
    int b_default_screen_size_ratio = 0;
    int master_screen_size_ratio_code = 0;
    int b_bed_object_chan_distribute = 0;
    int b_additional_data = 0;
    int add_data_bytes = 0;
    int add_data_bytes_minus1 = 0;
    // skip bed_render_info
  };

  struct Ac4SubstreamInfoChan {
    int channel_mode = 0;
    int b_4_back_channels_present = 0;
    int b_centre_present = 0;
    int top_channels_present = 0;
    int b_sf_multiplier = 0;
    int sf_multiplier = 0;
    int b_bitrate_info = 0;
    int bitrate_indicator = 0;
    int add_ch_base = 0;
    int b_audio_ndot = 0;
    int substream_index = 0;
  };

  struct Ac4SubstreamInfoAjoc {
    int b_lfe = 0;
    int b_static_dmx = 0;
    int n_fullband_dmx_signals = 0;
    int n_fullband_dmx_signals_minus1 = 0;
    // indent bed_dyn_obj_assignment
    int b_dyn_objects_only = 0;
    int b_isf = 0;
    int isf_config = 0;
    int b_ch_assign_code = 0;
    int bed_chan_assign_code = 0;
    int b_chan_assign_mask = 0;
    int b_nonstd_bed_channel_assignment = 0;
    int nonstd_bed_channel_assignment_mask = 0;
    int std_bed_channel_assignment_mask = 0;
    int n_bed_signals = 0;
    int n_bed_signals_minus1 = 0;
    int nonstd_bed_channel_assignment = 0;
    // de-indent
    int b_oamd_common_data_present = 0;
    OamdCommonData oamd_common_data;
    int n_fullband_upmix_signals_minus1 = 0;
    int b_sf_multiplier = 0;
    int sf_multiplier = 0;
    int b_bitrate_info = 0;
    int bitrate_indicator = 0;
    int b_audio_ndot = 0;
    int substream_index = 0;
  };

  struct Ac4SubstreamInfoObj {
    int n_objects_code = 0;
    int b_dynamic_objects = 0;
    int b_lfe = 0;
    int b_bed_objects = 0;
    int b_bed_start = 0;
    int b_ch_assign_code = 0;
    int bed_chan_assign_code = 0;
    int b_nonstd_bed_channel_assignment = 0;
    int nonstd_bed_channel_assignment_mask = 0;
    int std_bed_channel_assignment_mask = 0;
    int b_isf = 0;
    int b_isf_start = 0;
    int isf_config = 0;
    int b_sf_multiplier = 0;
    int sf_multiplier = 0;
    int b_bitrate_info = 0;
    int bitrate_indicator = 0;
    int b_audio_ndot = 0;
    int substream_index = 0;
  };

  struct Ac4PresentationV1Info {
    int b_single_substream_group = 0;
    int presentation_config = 0;
    int presentation_version = 0;
    int mdcompat = 0;
    int b_presentation_id = 0;
    FrameRateMultiplyInfo frame_rate_multiply_info;
    FrameRateFractionsInfo frame_rate_fractions_info;
    EmdfInfo emdf_info;
    int b_presentation_filter = 0;
    int b_enable_presentation = 0;
    int b_multi_pid = 0;
    // indent for ac4_sgi_specifier
    int group_index[16] = {0};
    //
    int n_substream_groups = 0;
    int n_substream_groups_minus2 = 0;
    int b_pre_virtualized = 0;
    int b_add_emdf_substreams = 0;
    int n_add_emdf_substreams = 0;
    // indent for ac4_presentation_substream_info
    int b_alternative = 0;
    int b_pres_ndot = 0;
    int substream_index = 0;
    EmdfInfo* emdf_infos;
  };

  struct Ac4SubstreamGroupInfo {
    int b_substreams_present = 0;
    int b_hsf_ext = 0;
    int b_single_substream = 0;
    int n_lf_substreams = 0;
    int n_lf_substreams_minus2 = 0;
    int b_channel_coded = 0;
    int sus_ver = 0;
    Ac4SubstreamInfoChan* substream_info_chan;
    // ac4_hsf_ext_substream_info
    int substream_index;
    int b_oamd_substream = 0;
    // oamd_substream_info
    int b_oamd_ndot = 0;
    int b_ajoc = 0;
    Ac4SubstreamInfoAjoc* substream_info_ajoc;
    Ac4SubstreamInfoObj* substream_info_obj;
    int b_content_type = 0;
    int content_classifier = 0;
    int b_language_indicator = 0;
    int b_serialized_language_tag = 0;
    int b_start_tag = 0;
    int language_tag_chunk = 0;
    int n_language_tag_bytes = 0;
    int language_tag_bytes[64] = {0};
  };

  struct Ac4Toc {
    int bitstream_version;
    int sequence_counter;
    int b_wait_frames;
    int wait_frames;
    int br_code;
    int fs_index;
    int frame_rate_index;
    int b_iframe_global;
    int b_single_presentation;
    int b_more_presentations;
    int b_payload_base;
    int payload_base_minus1;
    int b_program_id;
    int short_program_id;
    int b_program_uuid_present;
    int program_uuid;
    int n_presentations;
    int total_n_substream_groups;
    Ac4PresentationV1Info* presentation_v1_infos;
    Ac4SubstreamGroupInfo* substream_group_infos;
  };
  
  AC4Parser();
  virtual ~AC4Parser();

  virtual bool Parse(const uint8_t* data,
                     size_t data_size);
  int GetAc4TocSize() { return toc_size; };

 private:
  AC4Parser(const AC4Parser&) = delete;
  AC4Parser& operator=(const AC4Parser&) = delete;

  bool ParseAc4Toc(BitReader* reader);
  bool ParseAc4PresentationV1Info(BitReader* reader,
                                  Ac4PresentationV1Info& ac4_presentation_v1_info,
                                  int& max_group_index);
  bool ParseFrameRateMultiplyInfo(BitReader* reader);
  bool ParseFrameRateFractionsInfo(BitReader* reader);
  bool ParseEmdfInfo(BitReader* reader);
  bool ParseAc4PresentationSubstreamInfo(BitReader* reader);
  int ParseAc4SgiSpecifier(BitReader* reader);
  bool ParsePresentationConfigExtInfo(BitReader* reader);
  bool ParseAc4SubstreamGroupInfo(BitReader* reader,
                                  Ac4SubstreamGroupInfo& ac4_substream_group_info,
                                  int substream_group_index);
  bool ParseContentType(BitReader* reader,
                        Ac4SubstreamGroupInfo& ac4_substream_group_info);
  bool ParseOamdSubstreamInfo(BitReader* reader,
                              Ac4SubstreamGroupInfo& ac4_substream_group_info);
  bool ParseAc4HsfExtSubstreamInfo(BitReader* reader,
                                   Ac4SubstreamGroupInfo& ac4_substream_group_info);
  bool ParseAc4SubstreamInfoChan(BitReader* reader,
                                 int presentation_version,
                                 int fs_index,
                                 int frame_rate_factor,
                                 int b_substreams_present);
  int ParseChannelMode(BitReader* reader,
                                  int presentation_version);
  bool ParseAc4SubstreamInfoAjoc(BitReader* reader,
                                 int fs_index,
                                 int frame_rate_factor,
                                 int b_substreams_present);
  bool ParseBedDynObjAssignment(BitReader* reader,
                                int n_signals,
                                Ac4SubstreamInfoAjoc& ac4_substream_info_ajoc);
  bool ParseOamdCommonData(BitReader* reader);
  bool ParseAc4SubstreamInfoObj(BitReader* reader,
                                int fs_index,
                                int frame_rate_factor,
                                int b_substreams_present);
  int GetPresentationIdx(int substream_group_index);
  int GetPresentationVersion(int substream_group_index);
  bool ByteAlignment(BitReader* reader);
  
  int toc_size;
  Ac4Toc ac4_toc;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_AC4_PARSER_H_
