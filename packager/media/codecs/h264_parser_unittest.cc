// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "packager/base/logging.h"
#include "packager/media/codecs/h264_parser.h"
#include "packager/media/test/test_data_util.h"

namespace shaka {
namespace media {

namespace {
// SPS, PPS (for first slice), slice header from test-25fps.h264 file.
const uint8_t kSps[] = {
    0x27, 0x4D, 0x40, 0x0D, 0xA9, 0x18, 0x28, 0x3E, 0x60, 0x0D,
    0x41, 0x80, 0x41, 0xAD, 0xB0, 0xAD, 0x7B, 0xDF, 0x01,
};
const uint8_t kPps[] = {
    0x28, 0xDE, 0x9, 0x88,
};
// This is the prefix of a video slice that only has the header.
// The actual slice header size is 30 bits (not including the nalu header).
const uint8_t kVideoSliceTrimmed[] = {
    0x25, 0xB8, 0x20, 0x20, 0x63,
};
}  // namespace

TEST(H264ParserTest, StreamFileParsing) {
  std::vector<uint8_t> buffer = ReadTestDataFile("test-25fps.h264");

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
  H264Parser parser;
  int unused_id;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps, arraysize(kSps)));
  ASSERT_EQ(H264Parser::kOk, parser.ParseSps(nalu, &unused_id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kPps, arraysize(kPps)));
  ASSERT_EQ(H264Parser::kOk, parser.ParsePps(nalu, &unused_id));
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kVideoSliceTrimmed,
                              arraysize(kVideoSliceTrimmed)));

  H264SliceHeader slice_header;
  ASSERT_EQ(H264Parser::kOk, parser.ParseSliceHeader(nalu, &slice_header));
  EXPECT_EQ(nalu.data(), slice_header.nalu_data);
  EXPECT_EQ(30u, slice_header.header_bit_size);
}

TEST(H264ParserTest, ExtractResolutionFromSpsData) {
  const uint8_t kSps[] = {0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
                          0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
                          0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
                          0x60, 0x0F, 0x16, 0x2D, 0x96};

  H264Parser parser;
  int sps_id = 0;
  Nalu nalu;
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps, arraysize(kSps)));
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
  ASSERT_TRUE(nalu.Initialize(Nalu::kH264, kSps, arraysize(kSps)));
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
