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

TEST(H264ParserTest, StreamFileParsing) {
  std::vector<uint8_t> buffer = ReadTestDataFile("test-25fps.h264");

  // Number of NALUs in the test stream to be parsed.
  int num_nalus = 759;

  H264Parser parser;
  parser.SetStream(vector_as_array(&buffer), buffer.size());

  // Parse until the end of stream/unsupported stream/error in stream is found.
  int num_parsed_nalus = 0;
  while (true) {
    H264SliceHeader shdr;
    H264SEIMessage sei_msg;
    H264NALU nalu;
    H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res == H264Parser::kEOStream) {
      DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
               << num_parsed_nalus;
      ASSERT_EQ(num_nalus, num_parsed_nalus);
      return;
    }
    ASSERT_EQ(res, H264Parser::kOk);

    ++num_parsed_nalus;

    int id;
    switch (nalu.nal_unit_type) {
      case H264NALU::kIDRSlice:
      case H264NALU::kNonIDRSlice:
        ASSERT_EQ(parser.ParseSliceHeader(nalu, &shdr), H264Parser::kOk);
        break;

      case H264NALU::kSPS:
        ASSERT_EQ(parser.ParseSPS(&id), H264Parser::kOk);
        break;

      case H264NALU::kPPS:
        ASSERT_EQ(parser.ParsePPS(&id), H264Parser::kOk);
        break;

      case H264NALU::kSEIMessage:
        ASSERT_EQ(parser.ParseSEI(&sei_msg), H264Parser::kOk);
        break;

      default:
        // Skip unsupported NALU.
        DVLOG(4) << "Skipping unsupported NALU";
        break;
    }
  }
}

TEST(H264ParserTest, ExtractSarFromDecoderConfig) {
  const uint8_t kDecoderConfig[] = {
      0x01, 0x64, 0x00, 0x1E, 0xFF, 0xE1, 0x00, 0x1D, 0x67, 0x64, 0x00, 0x1E,
      0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA, 0x60, 0x0F, 0x16, 0x2D,
      0x96, 0x01, 0x00, 0x06, 0x68, 0xEB, 0xE3, 0xCB, 0x22, 0xC0};

  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  ExtractSarFromDecoderConfig(kDecoderConfig, arraysize(kDecoderConfig),
                              &pixel_width, &pixel_height);
  EXPECT_EQ(8u, pixel_width);
  EXPECT_EQ(9u, pixel_height);
}

TEST(H264ParserTest, ExtractSarFromSps) {
  const uint8_t kSps[] = {0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
                          0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
                          0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
                          0x60, 0x0F, 0x16, 0x2D, 0x96};

  uint32_t pixel_width = 0;
  uint32_t pixel_height = 0;
  ExtractSarFromSps(kSps, arraysize(kSps), &pixel_width, &pixel_height);
  EXPECT_EQ(8u, pixel_width);
  EXPECT_EQ(9u, pixel_height);
}

}  // namespace media
}  // namespace edash_packager
