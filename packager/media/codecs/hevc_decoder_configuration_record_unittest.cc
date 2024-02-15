// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/hevc_decoder_configuration_record.h>

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(HEVCDecoderConfigurationRecordTest, Success) {
  // clang-format off
  const uint8_t kHevcDecoderConfigurationData[] = {
      0x01,  // Version
      0x02,  // profile_indication
      0x20, 0x00, 0x00, 0x00,  // general_profile_compatibility_flags
      // general_constraint_indicator_flags
      0x90, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3F,  // general_level_idc
      0xF0, 0x00, 0xFC, 0xFD, 0xFA, 0xFA, 0x00, 0x00,
      0x0F,  // length_size_minus_one
      0x02,  // num_of_arrays
        // array 1
        0x20,  // nal type
        0x00, 0x01,  // num nalus
          // nalu 1
          0x00, 0x18,  // nal unit length
          0x40, 0x01, 0x0C, 0x01, 0xFF, 0xFF, 0x02, 0x20,
          0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
          0x00, 0x3F, 0x99, 0x98, 0x09,
        // array 2
        0x21,  // nal type
        0x00, 0x01,  // num nalus
          // nalu 1
          0x00, 0x24,  // nal unit length
          0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x3f, 0xa0, 0x05, 0x02, 0x01, 0x69, 0x65, 0x95,
          0xe4, 0x93, 0x2b, 0xc0, 0x40, 0x40, 0x00, 0x00, 0xfa, 0x40, 0x00,
          0x1d, 0x4c, 0x02,
  };
  // clang-format on
  HEVCDecoderConfigurationRecord hevc_config;
  ASSERT_TRUE(hevc_config.Parse(kHevcDecoderConfigurationData,
                                std::size(kHevcDecoderConfigurationData)));
  EXPECT_EQ(4u, hevc_config.nalu_length_size());
  EXPECT_EQ("hev1.2.4.L63.90", hevc_config.GetCodecString(FOURCC_hev1));
  EXPECT_EQ("hvc1.2.4.L63.90", hevc_config.GetCodecString(FOURCC_hvc1));
  EXPECT_EQ(2u, hevc_config.nalu_count());
  EXPECT_EQ(0x16u, hevc_config.nalu(0).payload_size());
  EXPECT_EQ(0x40, hevc_config.nalu(0).data()[0]);
}

TEST(HEVCDecoderConfigurationRecordTest, SuccessWithTransferCharacteristics) {
  // clang-format off
  const uint8_t kHevcDecoderConfigurationData[] = {
      0x01,  // Version
      0x02,  // profile_indication
      0x20, 0x00, 0x00, 0x00,  // general_profile_compatibility_flags
      // general_constraint_indicator_flags
      0x90, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3F,  // general_level_idc
      0xF0, 0x00, 0xFC, 0xFD, 0xFA, 0xFA, 0x00, 0x00,
      0x0F,  // length_size_minus_one
      0x03,  // num_of_arrays
        // array 1
        0xA0,  // nal type
        0x00, 0x01,  // num nalus
          0x00, 0x18,  // nal unit length
          0x40, 0x01, 0x0C, 0x01, 0xFF, 0xFF, 0x02, 0x20, 0x00, 0x00, 0x03,
          0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0x95,
          0x98, 0x09,
        // array 2
        0xA1,  // nal type
        0x00, 0x01,  // num nalus
          0x00, 0x38,  // nal unit length
           0x42, 0x01, 0x01, 0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00,
           0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0xA0, 0x03, 0xC0, 0x80,
           0x10, 0xE4, 0xD9, 0x65, 0x66, 0x92, 0x4C, 0xAF, 0x01, 0x6A, 0x12,
           0x20, 0x13, 0x6C, 0x20, 0x00, 0x00, 0x7D, 0x20, 0x00, 0x0B, 0xB8,
           0x0C, 0x25, 0x9A, 0x4B, 0xC0, 0x01, 0xE8, 0x48, 0x00, 0x3D, 0x09,
           0x10,
        // array 3
        0xA2,  // nal type
        0x00, 0x01,  // num nalus
          0x00, 0x07,  // nal unit length
          0x44, 0x01, 0xC1, 0x72, 0xA6, 0x46, 0x24,
  };
  // clang-format on

  HEVCDecoderConfigurationRecord hevc_config;
  ASSERT_TRUE(hevc_config.Parse(kHevcDecoderConfigurationData,
                                std::size(kHevcDecoderConfigurationData)));

  EXPECT_EQ(4u, hevc_config.nalu_length_size());

  EXPECT_EQ("hev1.2.4.L63.90", hevc_config.GetCodecString(FOURCC_hev1));
  EXPECT_EQ("hvc1.2.4.L63.90", hevc_config.GetCodecString(FOURCC_hvc1));

  EXPECT_EQ(3u, hevc_config.nalu_count());
  EXPECT_EQ(0x16u, hevc_config.nalu(0).payload_size());
  EXPECT_EQ(0x40, hevc_config.nalu(0).data()[0]);

  EXPECT_EQ(16, hevc_config.transfer_characteristics());
}

TEST(HEVCDecoderConfigurationRecordTest, FailOnInsufficientData) {
  const uint8_t kHevcDecoderConfigurationData[] = {0x01, 0x02, 0x20, 0x00};

  HEVCDecoderConfigurationRecord hevc_config;
  ASSERT_FALSE(hevc_config.Parse(kHevcDecoderConfigurationData,
                                 std::size(kHevcDecoderConfigurationData)));
}

}  // namespace media
}  // namespace shaka
