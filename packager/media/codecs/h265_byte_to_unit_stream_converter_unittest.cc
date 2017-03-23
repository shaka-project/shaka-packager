// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>
#include <stdio.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/codecs/h265_byte_to_unit_stream_converter.h"
#include "packager/media/codecs/hevc_decoder_configuration_record.h"
#include "packager/media/test/test_data_util.h"

namespace {
const char kExpectedConfigRecord[] =
    "01016000000300900000030000f000fcfdf8f800000303a10001002e42010101600000"
    "030090000003000003005da0028080241f265999a4932bffc0d5c0d640400000030040"
    "00000602a2000100074401c172b46240a00001001840010c01ffff0160000003009000"
    "0003000003005d999809";
}

namespace shaka {
namespace media {

TEST(H265ByteToUnitStreamConverter, StripParameterSetsNalu) {
  std::vector<uint8_t> input_frame =
      ReadTestDataFile("hevc-byte-stream-frame.h265");
  ASSERT_FALSE(input_frame.empty());

  std::vector<uint8_t> expected_output_frame =
      ReadTestDataFile("hvc1-unit-stream-frame.h265");
  ASSERT_FALSE(expected_output_frame.empty());

  H265ByteToUnitStreamConverter converter(
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

  // Double-check that it can be parsed.
  HEVCDecoderConfigurationRecord config;
  ASSERT_TRUE(config.Parse(decoder_config));
  // The order is SPS, PPS, VPS.
  ASSERT_EQ(3u, config.nalu_count());
  EXPECT_EQ(Nalu::H265_SPS, config.nalu(0).type());
  EXPECT_EQ(Nalu::H265_PPS, config.nalu(1).type());
  EXPECT_EQ(Nalu::H265_VPS, config.nalu(2).type());
}

TEST(H265ByteToUnitStreamConverter, KeepParameterSetsNalu) {
  std::vector<uint8_t> input_frame =
      ReadTestDataFile("hevc-byte-stream-frame.h265");
  ASSERT_FALSE(input_frame.empty());

  std::vector<uint8_t> expected_output_frame =
      ReadTestDataFile("hev1-unit-stream-frame.h265");
  ASSERT_FALSE(expected_output_frame.empty());

  H265ByteToUnitStreamConverter converter(
      H26xStreamFormat::kNalUnitStreamWithParameterSetNalus);
  std::vector<uint8_t> output_frame;
  ASSERT_TRUE(converter.ConvertByteStreamToNalUnitStream(input_frame.data(),
                                                         input_frame.size(),
                                                         &output_frame));
  EXPECT_EQ(expected_output_frame, output_frame);
}

TEST(H265ByteToUnitStreamConverter, ConversionFailure) {
  std::vector<uint8_t> input_frame(100, 0);

  H265ByteToUnitStreamConverter converter(
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

}  // namespace media
}  // namespace shaka
