// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>
#include <stdio.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/codecs/h264_byte_to_unit_stream_converter.h"
#include "packager/media/test/test_data_util.h"

namespace {
const char kExpectedConfigRecord[] =
    "014d400dffe10013274d400da918283e600d418041adb0ad7bdf01010004"
    "28de0988";
}

namespace shaka {
namespace media {

TEST(H264ByteToUnitStreamConverter, StripParameterSetsNalu) {
  std::vector<uint8_t> input_frame =
      ReadTestDataFile("avc-byte-stream-frame.h264");
  ASSERT_FALSE(input_frame.empty());

  std::vector<uint8_t> expected_output_frame =
      ReadTestDataFile("avc1-unit-stream-frame.h264");
  ASSERT_FALSE(expected_output_frame.empty());

  H264ByteToUnitStreamConverter converter(
      H26xStreamFormat::kNalUnitStreamWithoutParameterSetNalus);
  std::vector<uint8_t> output_frame;
  ASSERT_TRUE(converter.ConvertByteStreamToNalUnitStream(input_frame.data(),
                                                         input_frame.size(),
                                                         &output_frame));
  EXPECT_EQ(expected_output_frame, output_frame);

  std::vector<uint8_t> expected_decoder_config;
  ASSERT_TRUE(base::HexStringToBytes(kExpectedConfigRecord,
                                     &expected_decoder_config));
  std::vector<uint8_t> decoder_config;
  ASSERT_TRUE(converter.GetDecoderConfigurationRecord(&decoder_config));
  EXPECT_EQ(expected_decoder_config, decoder_config);
}

TEST(H264ByteToUnitStreamConverter, KeepParameterSetsNalu) {
  std::vector<uint8_t> input_frame =
      ReadTestDataFile("avc-byte-stream-frame.h264");
  ASSERT_FALSE(input_frame.empty());

  std::vector<uint8_t> expected_output_frame =
      ReadTestDataFile("avc3-unit-stream-frame.h264");
  ASSERT_FALSE(expected_output_frame.empty());

  H264ByteToUnitStreamConverter converter(
      H26xStreamFormat::kNalUnitStreamWithParameterSetNalus);
  std::vector<uint8_t> output_frame;
  ASSERT_TRUE(converter.ConvertByteStreamToNalUnitStream(input_frame.data(),
                                                         input_frame.size(),
                                                         &output_frame));
  EXPECT_EQ(expected_output_frame, output_frame);
}

TEST(H264ByteToUnitStreamConverter, ConversionFailure) {
  std::vector<uint8_t> input_frame(100, 0);

  H264ByteToUnitStreamConverter converter(
      H26xStreamFormat::kNalUnitStreamWithParameterSetNalus);
  std::vector<uint8_t> output_frame;
  EXPECT_FALSE(converter.ConvertByteStreamToNalUnitStream(input_frame.data(),
                                                          0,
                                                          &output_frame));
  EXPECT_FALSE(converter.ConvertByteStreamToNalUnitStream(input_frame.data(),
                                                          input_frame.size(),
                                                          &output_frame));
  std::vector<uint8_t> decoder_config;
  EXPECT_FALSE(converter.GetDecoderConfigurationRecord(&decoder_config));
}

TEST(H264ByteToUnitStreamConverter, NaluConversionWithSpsExtension) {
  const uint8_t kSpsExtArr[] = {
      0x00, 0x00, 0x00, 0x01,	// Start code
      0x09, 			// AUD Type
      0xF0, 			// Primary pic type
      0x00, 0x00, 0x00, 0x01, 	// Start code
      // Some SPS data
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91, 
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA, 
      0x60, 0x0F, 0x16, 0x2D, 0x96, 
      0x00, 0x00, 0x00, 0x01, 	// Start code
      // Some PPS data
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15, 
      0x00, 0x00, 0x00, 0x01,	// Start code
      // Some SPS Extension data
      0x6D, 0x33, 0x01, 0x57, 0x78, 
      0x00, 0x00, 0x00, 0x01, 	// Start code
      // Input NALU
      0x06, 
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, 
      0x00, 0x00, 0x00, 0x01, 	// Start code
      // SPS Data
      0x00, 0x02, 0x67, 0x64, 
      0x00, 0x00, 0x00, 0x01, 	// Start code
      // PPS Data
      0x00, 0x01, 0x68, 0xFE, 
      0x00, 0x00, 0x00, 0x01, 	// Start code
      // SPS Extension data
      0x00, 0x05, 0x6d, 0x33, 0x01, 0x57, 0x78
  };

  int kSpsExtSize = arraysize(kSpsExtArr);
  std::vector<uint8_t> kSpsExt(kSpsExtArr, kSpsExtArr + kSpsExtSize);
  H264ByteToUnitStreamConverter converter(
      H26xStreamFormat::kNalUnitStreamWithParameterSetNalus);

  std::vector<uint8_t> unit_stream_output;
  ASSERT_TRUE(converter.ConvertByteStreamToNalUnitStream(
              kSpsExt.data(), kSpsExt.size(),
              &unit_stream_output));                                                 
  std::vector<uint8_t> byte_stream_output;
  EXPECT_TRUE(converter.GetDecoderConfigurationRecord(&byte_stream_output));
}

}  // namespace media
}  // namespace shaka
