// Copyright 2014 The Chromium Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/codecs/h264_parser.h>

#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <packager/macros/logging.h>
#include <packager/media/test/test_data_util.h>

namespace shaka {
namespace media {

namespace {
// This is the prefix of a video slice (including the nalu header) that only has
// the slice header. The actual slice header size is 30 bits (not including the
// nalu header).
const uint8_t kVideoSliceTrimmed[] = {
    0x25, 0xB8, 0x20, 0x20, 0x63,
};

// This is another prefix of a video slice (including the nalu header).
// The slice header is 67 bits. So the first 10 bytes is the data before
// slice_data().
// Note also that this is from a real video slice and
// PPS's entropy_coding_mode_flag is true. So slice_data() starts from the 11th
// byte.
const uint8_t kVideoSliceTrimmedMultipleLumaWeights[] = {
    0x41, 0x9A, 0x72, 0x78, 0x43, 0xC9, 0x94, 0xC0,
    0x8C, 0xFF, 0xC1, 0x54,
};

// SPS for KVideoSliceTrimmedMultipleLumaWeights.
const uint8_t kSps2[] = {
    0x67, 0x64, 0x00, 0x28, 0xAC, 0xB2, 0x00, 0xF0, 0x04, 0x4F,
    0xCB, 0x80, 0xB5, 0x01, 0x01, 0x01, 0x40, 0x00, 0x00, 0x03,
    0x00, 0x40, 0x00, 0x00, 0x0F, 0x03, 0xC6, 0x0C, 0x92,
};

// PPS for KVideoSliceTrimmedMultipleLumaWeights.
const uint8_t kPps2[] = {
    0x68, 0xEB, 0xCC, 0xB2, 0x2C,
};
}  // namespace

TEST(H264ParserTest, StreamFileParsing) {
  std::vector<uint8_t> buffer = ReadTestDataFile("test-25fps.h264");
  ASSERT_FALSE(buffer.empty());

  // Number of NALUs in the test stream to be parsed.
  int num_nalus = 759;

  H264Parser parser;
  NaluReader reader(Nalu::kH264, kIsAnnexbByteStream, buffer.data(),
                    buffer.size());

  // Parse until the end of stream/unsupported stream/error in stream is found.
  int num_parsed_nalus = 0;
  while (true) {
    H264SliceHeader shdr;
    H264SEIMessage sei_msg;
    Nalu nalu;
    NaluReader::Result res = reader.Advance(&nalu);
    if (res == NaluReader::kEOStream) {
      DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
               << num_parsed_nalus;
      ASSERT_EQ(num_nalus, num_parsed_nalus);
      return;
    }
    ASSERT_EQ(res, NaluReader::kOk);

    ++num_parsed_nalus;

    int id;
    switch (nalu.type()) {
      case Nalu::H264_IDRSlice:
      case Nalu::H264_NonIDRSlice:
        ASSERT_EQ(parser.ParseSliceHeader(nalu, &shdr), H264Parser::kOk);
        break;

      case Nalu::H264_SPS:
        ASSERT_EQ(parser.ParseSps(nalu, &id), H264Parser::kOk);
        break;

      case Nalu::H264_PPS:
        ASSERT_EQ(parser.ParsePps(nalu, &id), H264Parser::kOk);
        break;

      case Nalu::H264_SEIMessage:
        ASSERT_EQ(parser.ParseSEI(nalu, &sei_msg), H264Parser::kOk);
        break;

      default:
        // Skip unsupported NALU.
        DVLOG(4) << "Skipping unsupported NALU";
        break;
    }
  }
}

// Verify that SliceHeader::nalu_data points to the beginning of nal unit.
// Also verify that header_bit_size is set correctly.
TEST(H264ParserTest, SliceHeaderSize) {
  const uint8_t kSps[] = {
      0x27, 0x4D, 0x40, 0x0D, 0xA9, 0x18, 0x28, 0x3E, 0x60, 0x0D,
      0x41, 0x80, 0x41, 0xAD, 0xB0, 0xAD, 0x7B, 0xDF, 0x01,
  };
  const uint8_t kPps[] = {
      0x28,
      0xDE,
      0x9,
      0x88,
  };

  H264Parser parser;
  int unused_id;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps, std::size(kSps)));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSps(nalu, &unused_id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kPps, std::size(kPps)));
  ASSERT_EQ(H264Parser::kOk, parser.ParsePps(nalu, &unused_id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kVideoSliceTrimmed,
                              std::size(kVideoSliceTrimmed)));

  H264SliceHeader slice_header;
  ASSERT_EQ(H264Parser::kOk, parser.ParseSliceHeader(nalu, &slice_header));
  EXPECT_EQ(nalu.data(), slice_header.nalu_data);
  EXPECT_EQ(30u, slice_header.header_bit_size);
}

TEST(H264ParserTest, PredWeightTable) {
  H264Parser parser;
  int unused_id;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps2, std::size(kSps2)));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSps(nalu, &unused_id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kPps2, std::size(kPps2)));
  ASSERT_EQ(H264Parser::kOk, parser.ParsePps(nalu, &unused_id));
  ASSERT_TRUE(
      nalu.Initialize(Nalu::kH264, kVideoSliceTrimmedMultipleLumaWeights,
                      std::size(kVideoSliceTrimmedMultipleLumaWeights)));

  H264SliceHeader slice_header;
  ASSERT_EQ(H264Parser::kOk, parser.ParseSliceHeader(nalu, &slice_header));

  EXPECT_TRUE(slice_header.num_ref_idx_active_override_flag);
  ASSERT_EQ(3, slice_header.num_ref_idx_l0_active_minus1);

  const H264WeightingFactors& pred_weight_table =
      slice_header.pred_weight_table_l0;

  EXPECT_FALSE(pred_weight_table.luma_weight_flag[0]);
  EXPECT_TRUE(pred_weight_table.luma_weight_flag[1]);
  EXPECT_FALSE(pred_weight_table.luma_weight_flag[2]);
  EXPECT_FALSE(pred_weight_table.luma_weight_flag[3]);

  // Luma checks.
  EXPECT_EQ(1, pred_weight_table.luma_weight[0]);
  EXPECT_EQ(1, pred_weight_table.luma_weight[1]);
  EXPECT_EQ(1, pred_weight_table.luma_weight[2]);
  EXPECT_EQ(1, pred_weight_table.luma_weight[3]);
  EXPECT_EQ(0, pred_weight_table.luma_offset[0]);
  EXPECT_EQ(-1, pred_weight_table.luma_offset[1]);
  EXPECT_EQ(0, pred_weight_table.luma_offset[2]);
  EXPECT_EQ(0, pred_weight_table.luma_offset[3]);

  EXPECT_FALSE(pred_weight_table.chroma_weight_flag[0]);
  EXPECT_FALSE(pred_weight_table.chroma_weight_flag[1]);
  EXPECT_FALSE(pred_weight_table.chroma_weight_flag[2]);
  EXPECT_FALSE(pred_weight_table.chroma_weight_flag[3]);

  // Chroma checks.
  // U plane.
  EXPECT_EQ(1, pred_weight_table.chroma_weight[0][0]);
  EXPECT_EQ(1, pred_weight_table.chroma_weight[1][0]);
  EXPECT_EQ(1, pred_weight_table.chroma_weight[2][0]);
  EXPECT_EQ(1, pred_weight_table.chroma_weight[3][0]);
  EXPECT_EQ(0, pred_weight_table.chroma_offset[0][0]);
  EXPECT_EQ(0, pred_weight_table.chroma_offset[1][0]);
  EXPECT_EQ(0, pred_weight_table.chroma_offset[2][0]);
  EXPECT_EQ(0, pred_weight_table.chroma_offset[3][0]);

  // V plane.
  EXPECT_EQ(1, pred_weight_table.chroma_weight[0][1]);
  EXPECT_EQ(1, pred_weight_table.chroma_weight[1][1]);
  EXPECT_EQ(1, pred_weight_table.chroma_weight[2][1]);
  EXPECT_EQ(1, pred_weight_table.chroma_weight[3][1]);
  EXPECT_EQ(0, pred_weight_table.chroma_offset[0][1]);
  EXPECT_EQ(0, pred_weight_table.chroma_offset[1][1]);
  EXPECT_EQ(0, pred_weight_table.chroma_offset[2][1]);
  EXPECT_EQ(0, pred_weight_table.chroma_offset[3][1]);
}

TEST(H264ParserTest, ParseSps) {
  const uint8_t kSps[] = {0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
                          0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
                          0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
                          0x60, 0x0F, 0x16, 0x2D, 0x96};

  H264Parser parser;
  int sps_id = 0;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps, std::size(kSps)));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSps(nalu, &sps_id));

  const H264Sps* sps = parser.GetSps(sps_id);
  ASSERT_TRUE(sps);

  EXPECT_EQ(100, sps->profile_idc);
  EXPECT_EQ(30, sps->level_idc);
  EXPECT_EQ(0, sps->transfer_characteristics);
}

TEST(H264ParserTest, ParseSpsWithTransferCharacteristics) {
  const uint8_t kSps[] = {
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
      0x00, 0x80, 0x00, 0x96, 0xA1, 0x22, 0x01, 0x28, 0x00, 0x00, 0x03, 0x00,
      0x08, 0x00, 0x00, 0x03, 0x01, 0x80, 0x78, 0xB1, 0x6C, 0xB0,
  };

  H264Parser parser;
  int sps_id = 0;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps, std::size(kSps)));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSps(nalu, &sps_id));

  const H264Sps* sps = parser.GetSps(sps_id);
  ASSERT_TRUE(sps);

  EXPECT_EQ(100, sps->profile_idc);
  EXPECT_EQ(30, sps->level_idc);
  EXPECT_EQ(16, sps->transfer_characteristics);
}

TEST(H264ParserTest, ExtractResolutionFromSpsData) {
  const uint8_t kSps[] = {0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
                          0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
                          0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
                          0x60, 0x0F, 0x16, 0x2D, 0x96};

  H264Parser parser;
  int sps_id = 0;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps, std::size(kSps)));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSps(nalu, &sps_id));

  uint32_t coded_width = 0;
  uint32_t coded_height = 0;
  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  ASSERT_TRUE(ExtractResolutionFromSps(*parser.GetSps(sps_id), &coded_width,
                                       &coded_height, &pixel_width,
                                       &pixel_height));
  EXPECT_EQ(720u, coded_width);
  EXPECT_EQ(360u, coded_height);
  EXPECT_EQ(8u, pixel_width);
  EXPECT_EQ(9u, pixel_height);
}

TEST(H264ParserTest, ExtractResolutionFromSpsDataWithCropping) {
  // 320x192 with frame_crop_bottom_offset of 6.
  const uint8_t kSps[] = {0x67, 0x64, 0x00, 0x0C, 0xAC, 0xD9, 0x41, 0x41, 0x9F,
                          0x9F, 0x01, 0x10, 0x00, 0x00, 0x03, 0x00, 0x10, 0x00,
                          0x00, 0x03, 0x03, 0x00, 0xF1, 0x42, 0x99, 0x60};

  H264Parser parser;
  int sps_id = 0;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps, std::size(kSps)));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSps(nalu, &sps_id));

  uint32_t coded_width = 0;
  uint32_t coded_height = 0;
  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  ASSERT_TRUE(ExtractResolutionFromSps(*parser.GetSps(sps_id), &coded_width,
                                       &coded_height, &pixel_width,
                                       &pixel_height));
  EXPECT_EQ(320u, coded_width);
  EXPECT_EQ(180u, coded_height);
  EXPECT_EQ(1u, pixel_width);
  EXPECT_EQ(1u, pixel_height);
}

}  // namespace media
}  // namespace shaka
