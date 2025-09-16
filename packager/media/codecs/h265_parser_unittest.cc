// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/h265_parser.h>

#include <gtest/gtest.h>

#include <packager/media/codecs/nalu_reader.h>

namespace shaka {
namespace media {
namespace H265 {

namespace {

// Data taken from bear-640x360-hevc.mp4 and bear-640x360-hevc-hdr10.mp4.
const uint8_t kSpsData[] = {
    0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3f, 0xa0, 0x05, 0x02, 0x01, 0x69, 0x65, 0x95, 0xe4, 0x93,
    0x2b, 0xc0, 0x40, 0x40, 0x00, 0x00, 0xfa, 0x40, 0x00, 0x1d, 0x4c, 0x02};
const uint8_t kSpsDataWithTransferCharacteristics[] = {
    0x42, 0x01, 0x01, 0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0xA0, 0x03, 0xC0, 0x80, 0x10, 0xE4,
    0xD9, 0x65, 0x66, 0x92, 0x4C, 0xAF, 0x01, 0x6A, 0x12, 0x20, 0x13, 0x6C,
    0x20, 0x00, 0x00, 0x7D, 0x20, 0x00, 0x0B, 0xB8, 0x0C, 0x25, 0x9A, 0x4B,
    0xC0, 0x01, 0xE8, 0x48, 0x00, 0x3D, 0x09, 0x10};
const uint8_t kPpsData[] = {0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89};
const uint8_t kSliceData[] = {
    // Incomplete segment data.
    0x26, 0x01, 0xaf, 0x08, 0x4c, 0x2e, 0xa6, 0x56, 0xd9, 0xaf, 0x50, 0xeb,
    0x94, 0x9a, 0xae, 0x89, 0x29, 0x0e, 0x42, 0x9f, 0xb9, 0x5e, 0x85, 0xd5};
const uint8_t kSliceData2[] = {0x02, 0x01, 0xd0, 0x29, 0xc9, 0xfd, 0x63, 0x22,
                               0x52, 0x04, 0x06, 0x13, 0x3d, 0xc6, 0xf0, 0xb9,
                               0x55, 0x98, 0xa0, 0x16, 0x57, 0xf6, 0xb8, 0x25};

const uint8_t kVpsData[] = {0x40, 0x01, 0x0C, 0x01, 0xFF, 0xFF, 0x01, 0x40,
                            0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03,
                            0x00, 0x00, 0x03, 0x00, 0x96, 0xBC, 0x09};
const uint8_t kSpsDataWithVps[] = {
    0x42, 0x01, 0x01, 0x01, 0x40, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x03, 0x00, 0x96, 0xA0, 0x03, 0xC0, 0x80, 0x11, 0x07,
    0xCB, 0x96, 0xF4, 0xA4, 0x21, 0x19, 0x3F, 0x8C, 0x04, 0x04, 0x00, 0x00,
    0x03, 0x00, 0x04, 0x00, 0x00, 0x03, 0x01, 0x68, 0x20};

// Stereo video sample.
const uint8_t kStereoVideoVpsData[] = {
    0x40, 0x01, 0x0C, 0x11, 0xFF, 0xFF, 0x21, 0x60, 0x00, 0x00, 0x03,
    0x00, 0xB0, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0x93,
    0x05, 0x6F, 0x78, 0x20, 0x00, 0x28, 0x24, 0x59, 0x72, 0x60, 0x20,
    0x00, 0x00, 0x03, 0x00, 0xF8, 0x80, 0x00, 0x00, 0x03, 0x00, 0x07,
    0x88, 0xD0, 0x78, 0x00, 0x44, 0x0A, 0x01, 0xE5, 0xC9, 0x7D, 0x20};
const uint8_t kStereoVideoSpsData0[] = {
    0x42, 0x01, 0x01, 0x21, 0x60, 0x00, 0x00, 0x03, 0x00, 0xB0,
    0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0xA0, 0x03,
    0xC0, 0x80, 0x11, 0x07, 0xCB, 0x96, 0x4E, 0xE4, 0xC9, 0xAE,
    0xD4, 0x94, 0x04, 0x04, 0x04, 0x0B, 0xB4, 0x28, 0x4D, 0x01};
const uint8_t kStereoVideoSpsData1[] = {0x42, 0x09, 0x0E, 0x85, 0xB9,
                                        0x32, 0x6B, 0xBE, 0x80, 0x2E,
                                        0xD0, 0xA1, 0x34, 0x04};
const uint8_t kStereoVideoPpsData0[] = {0x44, 0x01, 0xC5, 0xE3, 0x0F,
                                        0x09, 0xC1, 0x80, 0xC7, 0xB0,
                                        0x9A, 0x01, 0x40};
const uint8_t kStereoVideoPpsData1[] = {0x44, 0x09, 0x48, 0x5A, 0x43,
                                        0x0F, 0x09, 0xC1, 0x80, 0xC7,
                                        0xB4, 0x9A, 0x01, 0x40};
const uint8_t kStereoVideoSliceDataIntra0[] = {
    0x26, 0x01, 0xA3, 0xC6, 0xAC, 0x30, 0x02, 0x0C, 0xB0, 0x02,
    0x49, 0x88, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8};
const uint8_t kStereoVideoSliceDataIntra1[] = {
    0x26, 0x09, 0x90, 0x80, 0x3C, 0xC6, 0xAC, 0x30, 0x01, 0x20, 0x6A,
    0x01, 0x37, 0x28, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8};
const uint8_t kStereoVideoSliceDataInter0[] = {
    0x02, 0x01, 0xC4, 0x03, 0x7C, 0xC4, 0xAC, 0x30, 0x00, 0x0F, 0xAA,
    0x00, 0x1B, 0x30, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8};
const uint8_t kStereoVideoSliceDataInter1[] = {
    0x02, 0x09, 0xA1, 0x00, 0x9A, 0x00, 0xBD, 0x65, 0x89, 0x58, 0x60, 0x00,
    0x30, 0x2C, 0x00, 0x41, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0};

}  // namespace

TEST(H265ParserTest, ParseSliceHeader) {
  // Parse the SPS and PPS first so the data is available.
  int id;
  Nalu nalu;
  H265Parser parser;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsData, std::size(kSpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kPpsData, std::size(kPpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));

  // Parse the slice header.
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSliceData, std::size(kSliceData)));
  ASSERT_EQ(Nalu::H265_IDR_W_RADL, nalu.type());

  H265SliceHeader header;
  ASSERT_EQ(H265Parser::kOk, parser.ParseSliceHeader(nalu, &header));

  EXPECT_TRUE(header.first_slice_segment_in_pic_flag);
  EXPECT_EQ(0, header.pic_parameter_set_id);
  EXPECT_FALSE(header.dependent_slice_segment_flag);
  EXPECT_EQ(2, header.slice_type);
  EXPECT_EQ(8, header.slice_qp_delta);
  EXPECT_FALSE(header.cu_chroma_qp_offset_enabled_flag);
  EXPECT_EQ(5, header.num_entry_point_offsets);
  EXPECT_EQ(88u, header.header_bit_size);
}

TEST(H265ParserTest, ParseSliceHeader_NonIDR) {
  // Parse the SPS and PPS first so the data is available.
  int id;
  Nalu nalu;
  H265Parser parser;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsData, std::size(kSpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kPpsData, std::size(kPpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));

  // Parse the slice header.
  ASSERT_TRUE(
      nalu.Initialize(Nalu::kH265, kSliceData2, std::size(kSliceData2)));
  ASSERT_EQ(1 /* TRAIL_R */, nalu.type());

  H265SliceHeader header;
  ASSERT_EQ(H265Parser::kOk, parser.ParseSliceHeader(nalu, &header));

  EXPECT_TRUE(header.first_slice_segment_in_pic_flag);
  EXPECT_EQ(0, header.pic_parameter_set_id);
  EXPECT_FALSE(header.dependent_slice_segment_flag);
  EXPECT_EQ(1, header.slice_type);
  EXPECT_EQ(5, header.num_entry_point_offsets);
  EXPECT_EQ(128u, header.header_bit_size);
}

TEST(H265ParserTest, ParseSps) {
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsData, std::size(kSpsData)));
  ASSERT_EQ(Nalu::H265_SPS, nalu.type());

  int id = 12;
  H265Parser parser;
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));
  ASSERT_EQ(0, id);

  const H265Sps* sps = parser.GetSps(id);
  ASSERT_TRUE(sps);

  EXPECT_EQ(0, sps->video_parameter_set_id);
  EXPECT_EQ(0, sps->max_sub_layers_minus1);
  EXPECT_EQ(0, sps->seq_parameter_set_id);
  EXPECT_EQ(1, sps->chroma_format_idc);
  EXPECT_EQ(360, sps->pic_height_in_luma_samples);
  EXPECT_EQ(4, sps->log2_max_pic_order_cnt_lsb_minus4);
  EXPECT_EQ(3, sps->log2_diff_max_min_luma_transform_block_size);
  EXPECT_EQ(0, sps->max_transform_hierarchy_depth_intra);
  EXPECT_EQ(0, sps->vui_parameters.transfer_characteristics);
}

TEST(H265ParserTest, ParseSpsWithTransferCharacteristics) {
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsDataWithTransferCharacteristics,
                              std::size(kSpsDataWithTransferCharacteristics)));
  ASSERT_EQ(Nalu::H265_SPS, nalu.type());

  int id = 12;
  H265Parser parser;
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));
  ASSERT_EQ(0, id);

  const H265Sps* sps = parser.GetSps(id);
  ASSERT_TRUE(sps);

  EXPECT_EQ(0, sps->video_parameter_set_id);
  EXPECT_EQ(0, sps->max_sub_layers_minus1);
  EXPECT_EQ(0, sps->seq_parameter_set_id);
  EXPECT_EQ(1, sps->chroma_format_idc);
  EXPECT_EQ(1080, sps->pic_height_in_luma_samples);
  EXPECT_EQ(4, sps->log2_max_pic_order_cnt_lsb_minus4);
  EXPECT_EQ(3, sps->log2_diff_max_min_luma_transform_block_size);
  EXPECT_EQ(0, sps->max_transform_hierarchy_depth_intra);
  EXPECT_EQ(16, sps->vui_parameters.transfer_characteristics);
}

TEST(H265ParserTest, ParsePps) {
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kPpsData, std::size(kPpsData)));
  ASSERT_EQ(Nalu::H265_PPS, nalu.type());

  int id = 12;
  H265Parser parser;
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));
  ASSERT_EQ(0, id);

  const H265Pps* pps = parser.GetPps(id);
  ASSERT_TRUE(pps);

  EXPECT_EQ(0, pps->num_extra_slice_header_bits);
  EXPECT_TRUE(pps->weighted_pred_flag);
  EXPECT_FALSE(pps->scaling_list_data_present_flag);
  EXPECT_EQ(0, pps->log2_parallel_merge_level_minus2);
}

TEST(H265ParserTest, ExtractResolutionFromSpsData) {
  H265Parser parser;
  int sps_id = 0;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsData, std::size(kSpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &sps_id));

  uint32_t coded_width = 0;
  uint32_t coded_height = 0;
  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  ASSERT_TRUE(ExtractResolutionFromSps(*parser.GetSps(sps_id), &coded_width,
                                       &coded_height, &pixel_width,
                                       &pixel_height));
  EXPECT_EQ(640u, coded_width);
  EXPECT_EQ(360u, coded_height);
  EXPECT_EQ(1u, pixel_width);
  EXPECT_EQ(1u, pixel_height);
}

TEST(H265ParserTest, ExtractResolutionFromSpsDataWithCrop) {
  const uint8_t kSpsCropData[] = {
      0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00,
      0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x3c, 0xa0, 0x0f, 0x08, 0x0f,
      0x16, 0x59, 0x99, 0xa4, 0x93, 0x2b, 0xff, 0xc0, 0xd5, 0xc0, 0xd6,
      0x40, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40, 0x00, 0x00, 0x06, 0x02,
  };
  H265Parser parser;
  int sps_id = 0;
  Nalu nalu;
  ASSERT_TRUE(
      nalu.Initialize(Nalu::kH265, kSpsCropData, std::size(kSpsCropData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &sps_id));

  uint32_t coded_width = 0;
  uint32_t coded_height = 0;
  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  ASSERT_TRUE(ExtractResolutionFromSps(*parser.GetSps(sps_id), &coded_width,
                                       &coded_height, &pixel_width,
                                       &pixel_height));
  EXPECT_EQ(480u, coded_width);
  EXPECT_EQ(240u, coded_height);
  EXPECT_EQ(855u, pixel_width);
  EXPECT_EQ(857u, pixel_height);
}

TEST(H265ParserTest, ParseVps) {
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kVpsData, std::size(kVpsData)));
  ASSERT_EQ(Nalu::H265_VPS, nalu.type());

  int id = 0;
  H265Parser parser;
  ASSERT_EQ(H265Parser::kOk, parser.ParseVps(nalu, &id));
  ASSERT_EQ(0, id);

  const H265Vps* vps = parser.GetVps(id);
  ASSERT_TRUE(vps);

  EXPECT_EQ(0, vps->vps_max_layers_minus1);
  EXPECT_EQ(0, vps->vps_max_layer_id);
  EXPECT_EQ(0, vps->vps_num_layer_sets_minus1);
}

TEST(H265ParserTest, VpsProfileTierLevelMatchesSpsProfileTierLevel) {
  // Parse the VPS and SPS and check their general_profile_tier_level_data.
  int id;
  Nalu nalu;
  H265Parser parser;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kVpsData, std::size(kVpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseVps(nalu, &id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsDataWithVps,
                              std::size(kSpsDataWithVps)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));

  const H265Sps* sps = parser.GetSps(id);
  ASSERT_TRUE(sps);
  const H265Vps* vps = parser.GetVps(sps->video_parameter_set_id);
  ASSERT_TRUE(vps);

  int general_tier_flag_sps =
      (sps->general_profile_tier_level_data[0] >> 5) & 0x01;
  int general_profile_idc_sps = sps->general_profile_tier_level_data[0] & 0x1F;
  int general_level_idc_sps = sps->general_profile_tier_level_data[11];
  int general_tier_flag_vps =
      (vps->general_profile_tier_level_data[0][0] >> 5) & 0x01;
  int general_profile_idc_vps =
      vps->general_profile_tier_level_data[0][0] & 0x1F;
  int general_level_idc_vps = vps->general_profile_tier_level_data[0][11];
  EXPECT_EQ(general_tier_flag_sps, general_tier_flag_vps);
  EXPECT_EQ(general_profile_idc_sps, general_profile_idc_vps);
  EXPECT_EQ(general_level_idc_sps, general_level_idc_vps);
}

TEST(H265ParserTest, ParseStereoVideoVpsAndSps) {
  int id;
  Nalu nalu;
  H265Parser parser;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoVpsData,
                              std::size(kStereoVideoVpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseVps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSpsData0,
                              std::size(kStereoVideoSpsData0)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));
  const H265Sps* sps0 = parser.GetSps(id);
  ASSERT_TRUE(sps0);

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSpsData1,
                              std::size(kStereoVideoSpsData1)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));
  const H265Sps* sps1 = parser.GetSps(id);
  ASSERT_TRUE(sps1);

  EXPECT_EQ(sps0->video_parameter_set_id, sps1->video_parameter_set_id);
  const H265Vps* vps = parser.GetVps(sps0->video_parameter_set_id);
  ASSERT_TRUE(vps);

  EXPECT_TRUE(vps->vps_base_layer_internal_flag);
  EXPECT_TRUE(vps->vps_base_layer_available_flag);

  // Expect 2 layers for stereo video.
  EXPECT_EQ(1, vps->vps_max_layers_minus1);
  EXPECT_EQ(1, vps->vps_max_layer_id);
  EXPECT_EQ(1, vps->vps_num_layer_sets_minus1);
  EXPECT_EQ(0, vps->layer_id_in_vps[0]);
  EXPECT_EQ(1, vps->layer_id_in_vps[1]);

  // Expect 2 views for stereo video.
  EXPECT_EQ(2, vps->num_views);
  EXPECT_EQ(0, vps->view_id[0]);
  EXPECT_EQ(1, vps->view_id[1]);

  EXPECT_EQ(3, vps->num_profile_tier_levels);
  EXPECT_EQ(1, vps->num_rep_formats);

  // Base layer (primary view) profile_idc.
  EXPECT_EQ(1, (sps0->general_profile_tier_level_data[0] & 0x1F));
  // Layer 1 (secondary view) profile_idc.
  EXPECT_EQ(6, (sps1->general_profile_tier_level_data[0] & 0x1F));

  // Expect the following to be the same between the two SPSs
  EXPECT_EQ(sps0->chroma_format_idc, sps1->chroma_format_idc);
  EXPECT_EQ(sps0->separate_colour_plane_flag, sps1->separate_colour_plane_flag);
  EXPECT_EQ(sps0->pic_width_in_luma_samples, sps1->pic_width_in_luma_samples);
  EXPECT_EQ(sps0->pic_height_in_luma_samples, sps1->pic_height_in_luma_samples);
  EXPECT_EQ(sps0->conf_win_left_offset, sps1->conf_win_left_offset);
  EXPECT_EQ(sps0->conf_win_right_offset, sps1->conf_win_right_offset);
  EXPECT_EQ(sps0->conf_win_top_offset, sps1->conf_win_top_offset);
  EXPECT_EQ(sps0->conf_win_bottom_offset, sps1->conf_win_bottom_offset);
  EXPECT_EQ(sps0->bit_depth_luma_minus8, sps1->bit_depth_luma_minus8);
  EXPECT_EQ(sps0->bit_depth_chroma_minus8, sps1->bit_depth_chroma_minus8);

  // This is from 8-bit test content.
  EXPECT_EQ(0, sps1->bit_depth_luma_minus8);
  EXPECT_EQ(0, sps1->bit_depth_chroma_minus8);
}

TEST(H265ParserTest, ParseStereoVideoSliceHeaderIntra) {
  // Parse the VPS, SPS and PPS first so the data is available.
  int id;
  Nalu nalu;
  H265Parser parser;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoVpsData,
                              std::size(kStereoVideoVpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseVps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSpsData0,
                              std::size(kStereoVideoSpsData0)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSpsData1,
                              std::size(kStereoVideoSpsData1)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoPpsData0,
                              std::size(kStereoVideoPpsData0)));
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoPpsData1,
                              std::size(kStereoVideoPpsData1)));
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));

  // Parse the slice header for layer 0.
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSliceDataIntra0,
                              std::size(kStereoVideoSliceDataIntra0)));
  H265SliceHeader header0;
  ASSERT_EQ(H265Parser::kOk, parser.ParseSliceHeader(nalu, &header0));
  EXPECT_EQ(0, nalu.nuh_layer_id());
  EXPECT_TRUE(header0.first_slice_segment_in_pic_flag);
  EXPECT_EQ(0, header0.pic_parameter_set_id);
  EXPECT_FALSE(header0.dependent_slice_segment_flag);
  EXPECT_EQ(2, header0.slice_type);
  EXPECT_EQ(136u, header0.header_bit_size);

  // Parse the slice header for layer 1.
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSliceDataIntra1,
                              std::size(kStereoVideoSliceDataIntra1)));
  H265SliceHeader header1;
  ASSERT_EQ(H265Parser::kOk, parser.ParseSliceHeader(nalu, &header1));
  const H265Pps* pps = parser.GetPps(header1.pic_parameter_set_id);
  ASSERT_TRUE(pps);
  const H265Sps* sps = parser.GetSps(pps->seq_parameter_set_id);
  ASSERT_TRUE(sps);
  const H265Vps* vps = parser.GetVps(sps->video_parameter_set_id);
  ASSERT_TRUE(vps);
  EXPECT_EQ(1, vps->num_direct_ref_layers[1]);

  EXPECT_EQ(1, nalu.nuh_layer_id());
  EXPECT_TRUE(header1.first_slice_segment_in_pic_flag);
  EXPECT_EQ(1, header1.pic_parameter_set_id);
  EXPECT_FALSE(header1.dependent_slice_segment_flag);
  EXPECT_EQ(1, header1.slice_type);
  EXPECT_EQ(152u, header1.header_bit_size);
}

TEST(H265ParserTest, ParseStereoVideoSliceHeaderInter) {
  // Parse the SPS and PPS first so the data is available.
  int id;
  Nalu nalu;
  H265Parser parser;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoVpsData,
                              std::size(kStereoVideoVpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseVps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSpsData0,
                              std::size(kStereoVideoSpsData0)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSpsData1,
                              std::size(kStereoVideoSpsData1)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoPpsData0,
                              std::size(kStereoVideoPpsData0)));
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));

  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoPpsData1,
                              std::size(kStereoVideoPpsData1)));
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));

  // Parse the slice header for layer 0.
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSliceDataInter0,
                              std::size(kStereoVideoSliceDataInter0)));
  H265SliceHeader header0;
  ASSERT_EQ(H265Parser::kOk, parser.ParseSliceHeader(nalu, &header0));
  EXPECT_EQ(0, nalu.nuh_layer_id());
  EXPECT_TRUE(header0.first_slice_segment_in_pic_flag);
  EXPECT_EQ(0, header0.pic_parameter_set_id);
  EXPECT_FALSE(header0.dependent_slice_segment_flag);
  EXPECT_EQ(1, header0.slice_type);
  EXPECT_EQ(152u, header0.header_bit_size);

  // Parse the slice header for layer 1.
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kStereoVideoSliceDataInter1,
                              std::size(kStereoVideoSliceDataInter1)));
  H265SliceHeader header1;
  ASSERT_EQ(H265Parser::kOk, parser.ParseSliceHeader(nalu, &header1));
  const H265Pps* pps = parser.GetPps(header1.pic_parameter_set_id);
  ASSERT_TRUE(pps);
  const H265Sps* sps = parser.GetSps(pps->seq_parameter_set_id);
  ASSERT_TRUE(sps);
  const H265Vps* vps = parser.GetVps(sps->video_parameter_set_id);
  ASSERT_TRUE(vps);
  EXPECT_EQ(1, vps->num_direct_ref_layers[1]);

  EXPECT_EQ(1, nalu.nuh_layer_id());
  EXPECT_TRUE(header1.first_slice_segment_in_pic_flag);
  EXPECT_EQ(1, header1.pic_parameter_set_id);
  EXPECT_FALSE(header1.dependent_slice_segment_flag);
  EXPECT_EQ(1, header1.slice_type);
  EXPECT_EQ(176u, header1.header_bit_size);
}

}  // namespace H265
}  // namespace media
}  // namespace shaka
