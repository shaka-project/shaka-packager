// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/codecs/h265_parser.h"
#include "packager/media/codecs/nalu_reader.h"

namespace shaka {
namespace media {
namespace H265 {

namespace {

// Data taken from bear-640x360-hevc.mp4
const uint8_t kSpsData[] = {
    0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3f, 0xa0, 0x05, 0x02, 0x01, 0x69, 0x65, 0x95, 0xe4, 0x93,
    0x2b, 0xc0, 0x40, 0x40, 0x00, 0x00, 0xfa, 0x40, 0x00, 0x1d, 0x4c, 0x02};
const uint8_t kPpsData[] = {0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89};
const uint8_t kSliceData[] = {
    // Incomplete segment data.
    0x26, 0x01, 0xaf, 0x08, 0x4c, 0x2e, 0xa6, 0x56, 0xd9, 0xaf, 0x50, 0xeb,
    0x94, 0x9a, 0xae, 0x89, 0x29, 0x0e, 0x42, 0x9f, 0xb9, 0x5e, 0x85, 0xd5};
const uint8_t kSliceData2[] = {0x02, 0x01, 0xd0, 0x29, 0xc9, 0xfd, 0x63, 0x22,
                               0x52, 0x04, 0x06, 0x13, 0x3d, 0xc6, 0xf0, 0xb9,
                               0x55, 0x98, 0xa0, 0x16, 0x57, 0xf6, 0xb8, 0x25};

}  // namespace

TEST(H265ParserTest, ParseSliceHeader) {
  // Parse the SPS and PPS first so the data is available.
  int id;
  Nalu nalu;
  H265Parser parser;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsData, arraysize(kSpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kPpsData, arraysize(kPpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));

  // Parse the slice header.
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSliceData, arraysize(kSliceData)));
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
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsData, arraysize(kSpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParseSps(nalu, &id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kPpsData, arraysize(kPpsData)));
  ASSERT_EQ(H265Parser::kOk, parser.ParsePps(nalu, &id));

  // Parse the slice header.
  ASSERT_TRUE(
      nalu.Initialize(Nalu::kH265, kSliceData2, arraysize(kSliceData2)));
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
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsData, arraysize(kSpsData)));
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
}

TEST(H265ParserTest, ParsePps) {
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kPpsData, arraysize(kPpsData)));
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
  ASSERT_TRUE(nalu.Initialize(Nalu::kH265, kSpsData, arraysize(kSpsData)));
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
      nalu.Initialize(Nalu::kH265, kSpsCropData, arraysize(kSpsCropData)));
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

}  // namespace H265
}  // namespace media
}  // namespace shaka
