// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/h264_byte_to_unit_stream_converter.h>

#include <absl/strings/escaping.h>
#include <gtest/gtest.h>

#include <packager/media/test/test_data_util.h>

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

  auto expected_decoder_config_str =
      absl::HexStringToBytes(kExpectedConfigRecord);
  ASSERT_FALSE(expected_decoder_config_str.empty());
  std::vector<uint8_t> expected_decoder_config(
      expected_decoder_config_str.begin(), expected_decoder_config_str.end());
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
  const uint8_t kByteStreamWithSpsExtension[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code
      0x09,  		       // AUD Type
      0xF0, 		       // Primary pic type
      0x00, 0x00, 0x00, 0x01,  // Start code
      // Some SPS Data
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 
      0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91, 0x00, 0x00, 
      0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA, 0x60, 0x0F, 0x16, 
      0x2D, 0x96, 
      0x00, 0x00, 0x00, 0x01,  // Start code
      // Some PPS Data
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15, 
      0x00, 0x00, 0x00, 0x01,  // Start code
      // Some SPS Extension data
      0x6D, 0x33, 0x01, 0x57, 0x78, 
      0x00, 0x00, 0x00, 0x01,  // Start code
      // The input NALU
      0x06,  //  NALU type
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, 
  };
  std::vector<uint8_t> byte_stream_with_sps_extension(
  	std::begin(kByteStreamWithSpsExtension), 
  	std::end(kByteStreamWithSpsExtension));
  	
  H264ByteToUnitStreamConverter converter(
      H26xStreamFormat::kNalUnitStreamWithoutParameterSetNalus);

  std::vector<uint8_t> unit_stream;
  ASSERT_TRUE(converter.ConvertByteStreamToNalUnitStream(
              byte_stream_with_sps_extension.data(), 
              byte_stream_with_sps_extension.size(), &unit_stream));

  const uint8_t kExpectedUnitStream[] = {
        0x00, 0x00, 0x00, 0x0A, 0x06, 0xFD, 0x78, 
        0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77
  };          
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kExpectedUnitStream), 
            std::end(kExpectedUnitStream)), unit_stream);

  std::cout << std::endl << std::endl;
  std::vector<uint8_t> decoder_config;
  EXPECT_TRUE(converter.GetDecoderConfigurationRecord(&decoder_config));

  const uint8_t kExpectedDecoderConfig[] = {
      0x01, 0x64, 0x00, 0x1E, 0xFF, 0xE1, 0x00, 0x1D, 
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91, 
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA, 
      0x60, 0x0F, 0x16, 0x2D, 0x96, 0x01, 0x00, 0x0A, 
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 
      0x14, 0x15, 0xFD, 0xF8, 0xF8, 0x01, 0x6D, 0x33, 
      0x01, 0x57, 0x78
  };
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kExpectedDecoderConfig), 
            std::end(kExpectedDecoderConfig)), decoder_config);          
}

}  // namespace media
}  // namespace shaka
