// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "base/strings/string_number_conversions.h"
#include "media/filters/h264_byte_to_unit_stream_converter.h"
#include "media/test/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <stdio.h>

namespace {
const char kExpectedConfigRecord[] =
    "014d400dffe10013274d400da918283e600d418041adb0ad7bdf01010004"
    "28de0988";
}

namespace media {

TEST(H264ByteToUnitStreamConverter, ConversionSuccess) {
  std::vector<uint8> input_frame =
      ReadTestDataFile("avc-byte-stream-frame.h264");
  ASSERT_FALSE(input_frame.empty());

  std::vector<uint8> expected_output_frame =
      ReadTestDataFile("avc-unit-stream-frame.h264");
  ASSERT_FALSE(expected_output_frame.empty());

  H264ByteToUnitStreamConverter converter;
  std::vector<uint8> output_frame;
  ASSERT_TRUE(converter.ConvertByteStreamToNalUnitStream(input_frame.data(),
                                                         input_frame.size(),
                                                         &output_frame));
  EXPECT_EQ(expected_output_frame, output_frame);

  std::vector<uint8> expected_decoder_config;
  ASSERT_TRUE(base::HexStringToBytes(kExpectedConfigRecord,
                                     &expected_decoder_config));
  std::vector<uint8> decoder_config;
  ASSERT_TRUE(converter.GetAVCDecoderConfigurationRecord(&decoder_config));
  EXPECT_EQ(expected_decoder_config, decoder_config);
}

TEST(H264ByteToUnitStreamConverter, ConversionFailure) {
  std::vector<uint8> input_frame(100, 0);

  H264ByteToUnitStreamConverter converter;
  std::vector<uint8> output_frame;
  EXPECT_FALSE(converter.ConvertByteStreamToNalUnitStream(input_frame.data(),
                                                          0,
                                                          &output_frame));
  EXPECT_FALSE(converter.ConvertByteStreamToNalUnitStream(input_frame.data(),
                                                          input_frame.size(),
                                                          &output_frame));
  std::vector<uint8> decoder_config;
  EXPECT_FALSE(converter.GetAVCDecoderConfigurationRecord(&decoder_config));
}

}  // namespace media
