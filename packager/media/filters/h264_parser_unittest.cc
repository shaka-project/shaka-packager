// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "packager/base/logging.h"
#include "packager/base/stl_util.h"
#include "packager/media/test/test_data_util.h"
#include "packager/media/filters/h264_parser.h"

namespace edash_packager {
namespace media {

namespace {
// The test data does not include a start code, the start of the data is the
// NALU header.
const uint8_t kStartCodeSize = 0;
}

TEST(H264ParserTest, StreamFileParsing) {
  std::vector<uint8_t> buffer = ReadTestDataFile("test-25fps.h264");

  // Number of NALUs in the test stream to be parsed.
  int num_nalus = 759;

  H264Parser parser;
  NaluReader reader(kIsAnnexbByteStream, vector_as_array(&buffer),
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
        ASSERT_EQ(parser.ParseSPS(nalu, &id), H264Parser::kOk);
        break;

      case Nalu::H264_PPS:
        ASSERT_EQ(parser.ParsePPS(nalu, &id), H264Parser::kOk);
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

TEST(H264ParserTest, ExtractResolutionFromSpsData) {
  const uint8_t kSps[] = {0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
                          0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
                          0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
                          0x60, 0x0F, 0x16, 0x2D, 0x96};

  H264Parser parser;
  int sps_id = 0;
  Nalu nalu;
  ASSERT_TRUE(nalu.InitializeFromH264(kSps, arraysize(kSps), kStartCodeSize));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSPS(nalu, &sps_id));

  uint32_t coded_width = 0;
  uint32_t coded_height = 0;
  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  ASSERT_TRUE(ExtractResolutionFromSps(*parser.GetSPS(sps_id), &coded_width,
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
  ASSERT_TRUE(nalu.InitializeFromH264(kSps, arraysize(kSps), kStartCodeSize));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSPS(nalu, &sps_id));

  uint32_t coded_width = 0;
  uint32_t coded_height = 0;
  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  ASSERT_TRUE(ExtractResolutionFromSps(*parser.GetSPS(sps_id), &coded_width,
                                       &coded_height, &pixel_width,
                                       &pixel_height));
  EXPECT_EQ(320u, coded_width);
  EXPECT_EQ(180u, coded_height);
  EXPECT_EQ(1u, pixel_width);
  EXPECT_EQ(1u, pixel_height);
}

}  // namespace media
}  // namespace edash_packager
