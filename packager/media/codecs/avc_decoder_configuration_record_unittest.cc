// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/avc_decoder_configuration_record.h>

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(AVCDecoderConfigurationRecordTest, Success) {
  // clang-format off
  const uint8_t kAvcDecoderConfigurationData[] = {
      0x01,  // version
      0x64,  // profile_indication
      0x00,  // profile_compatibility
      0x1E,  // avc_level
      0xFF,  // Least significant 3 bits is length_size_minus_one
      0xE1,  // Least significant 5 bits is num_sps
        // sps 1
        0x00, 0x1D,  // size
        0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
        0x00, 0x80, 0x00, 0x91, 0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
        0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x01,  // num_pps
        0x00, 0x06,  // size
        0x68, 0xEB, 0xE3, 0xCB, 0x22, 0xC0,
  };
  // clang-format on

  AVCDecoderConfigurationRecord avc_config;
  ASSERT_TRUE(avc_config.Parse(kAvcDecoderConfigurationData,
                               std::size(kAvcDecoderConfigurationData)));

  EXPECT_EQ(1u, avc_config.version());
  EXPECT_EQ(0x64, avc_config.profile_indication());
  EXPECT_EQ(0u, avc_config.profile_compatibility());
  EXPECT_EQ(0x1E, avc_config.avc_level());
  EXPECT_EQ(4u, avc_config.nalu_length_size());
  EXPECT_EQ(720u, avc_config.coded_width());
  EXPECT_EQ(360u, avc_config.coded_height());
  EXPECT_EQ(8u, avc_config.pixel_width());
  EXPECT_EQ(9u, avc_config.pixel_height());
  EXPECT_EQ(0u, avc_config.transfer_characteristics());
  EXPECT_EQ(0u, avc_config.chroma_format());
  EXPECT_EQ(0u, avc_config.bit_depth_luma_minus8());
  EXPECT_EQ(0u, avc_config.bit_depth_chroma_minus8());

  EXPECT_EQ("avc1.64001e", avc_config.GetCodecString(FOURCC_avc1));
  EXPECT_EQ("avc3.64001e", avc_config.GetCodecString(FOURCC_avc3));
}

TEST(AVCDecoderConfigurationRecordTest, SuccessWithSPSExtension) {
  // clang-format off
  const uint8_t kAvcDecoderConfigurationData[] = {
      0x01,  // version
      0x64,  // profile_indication
      0x00,  // profile_compatibility
      0x1E,  // avc_level
      0xFF,  // Least significant 3 bits is length_size_minus_one
      0xE1,  // Least significant 5 bits is num_sps
        // sps 1
        0x00, 0x1D,  // size
        0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
        0x00, 0x80, 0x00, 0x91, 0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
        0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x01,  // num_pps
        0x00, 0x06,  // size
        0x68, 0xEB, 0xE3, 0xCB, 0x22, 0xC0,
      0xFC,  // Least significant 2 bits is chroma_format 
      0xF9,  // Least significant 3 bits is bit_depth_luma_minus8
      0xFF,  // Least significant 3 bits is bit_depth_chroma_minus8
      0x01,  // num_sps_ext 
        0x00, 0x05,  // size
        0x6D, 0x33, 0x01, 0x57, 0x78
  };
  // clang-format on

  AVCDecoderConfigurationRecord avc_config;
  ASSERT_TRUE(avc_config.Parse(kAvcDecoderConfigurationData,
                               std::size(kAvcDecoderConfigurationData)));

  EXPECT_EQ(1u, avc_config.version());
  EXPECT_EQ(0x64, avc_config.profile_indication());
  EXPECT_EQ(0u, avc_config.profile_compatibility());
  EXPECT_EQ(0x1E, avc_config.avc_level());
  EXPECT_EQ(4u, avc_config.nalu_length_size());
  EXPECT_EQ(720u, avc_config.coded_width());
  EXPECT_EQ(360u, avc_config.coded_height());
  EXPECT_EQ(8u, avc_config.pixel_width());
  EXPECT_EQ(9u, avc_config.pixel_height());
  EXPECT_EQ(0u, avc_config.transfer_characteristics());
  EXPECT_EQ(0u, avc_config.chroma_format());
  EXPECT_EQ(1u, avc_config.bit_depth_luma_minus8());
  EXPECT_EQ(7u, avc_config.bit_depth_chroma_minus8());
}

TEST(AVCDecoderConfigurationRecordTest, SuccessWithTransferCharacteristics) {
  // clang-format off
  const uint8_t kAvcDecoderConfigurationData[] = {
      0x01,  // version
      0x64,  // profile_indication
      0x00,  // profile_compatibility
      0x1E,  // avc_level
      0xFF,  // Least significant 3 bits is length_size_minus_one
      0xE1,  // Least significant 5 bits is num_sps
        // sps 1
        0x00, 0x22,  // size
        0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
        0x00, 0x80, 0x00, 0x96, 0xA1, 0x22, 0x01, 0x28, 0x00, 0x00, 0x03, 0x00,
        0x08, 0x00, 0x00, 0x03, 0x01, 0x80, 0x78, 0xB1, 0x6C, 0xB0,
      0x01,  // num_pps
        // pps 1
        0x00, 0x06,  // size
        0x68, 0xEB, 0xE1, 0x32, 0xC8, 0xB0,
  };
  // clang-format on

  AVCDecoderConfigurationRecord avc_config;
  ASSERT_TRUE(avc_config.Parse(kAvcDecoderConfigurationData,
                               std::size(kAvcDecoderConfigurationData)));

  EXPECT_EQ(1u, avc_config.version());
  EXPECT_EQ(0x64, avc_config.profile_indication());
  EXPECT_EQ(0u, avc_config.profile_compatibility());
  EXPECT_EQ(0x1E, avc_config.avc_level());
  EXPECT_EQ(4u, avc_config.nalu_length_size());
  EXPECT_EQ(720u, avc_config.coded_width());
  EXPECT_EQ(360u, avc_config.coded_height());
  EXPECT_EQ(8u, avc_config.pixel_width());
  EXPECT_EQ(9u, avc_config.pixel_height());
  EXPECT_EQ(16u, avc_config.transfer_characteristics());

  EXPECT_EQ("avc1.64001e", avc_config.GetCodecString(FOURCC_avc1));
  EXPECT_EQ("avc3.64001e", avc_config.GetCodecString(FOURCC_avc3));
}

TEST(AVCDecoderConfigurationRecordTest, SuccessWithNoParameterSets) {
  // clang-format off
  const uint8_t kAvcDecoderConfigurationData[] = {
      0x01,  // version
      0x64,  // profile_indication
      0x00,  // profile_compatibility
      0x1E,  // avc_level
      0xFF,  // Least significant 3 bits is length_size_minus_one
      0xE0,  // Least significant 5 bits is num_sps
      0x00,  // num_pps
  };
  // clang-format on

  AVCDecoderConfigurationRecord avc_config;
  ASSERT_TRUE(avc_config.Parse(kAvcDecoderConfigurationData,
                               std::size(kAvcDecoderConfigurationData)));

  EXPECT_EQ(1u, avc_config.version());
  EXPECT_EQ(0x64, avc_config.profile_indication());
  EXPECT_EQ(0u, avc_config.profile_compatibility());
  EXPECT_EQ(0x1E, avc_config.avc_level());
  EXPECT_EQ(4u, avc_config.nalu_length_size());
  EXPECT_EQ(0u, avc_config.coded_width());
  EXPECT_EQ(0u, avc_config.coded_height());
  EXPECT_EQ(0u, avc_config.pixel_width());
  EXPECT_EQ(0u, avc_config.pixel_height());
  EXPECT_EQ(0u, avc_config.transfer_characteristics());

  EXPECT_EQ("avc3.64001e", avc_config.GetCodecString(FOURCC_avc3));
}

TEST(AVCDecoderConfigurationRecordTest, FailsOnInvalidNaluLengthSize) {
  // clang-format off
  const uint8_t kAvcDecoderConfigurationData[] = {
      0x01,  // version
      0x64,  // profile_indication
      0x00,  // profile_compatibility
      0x1E,  // avc_level
      0xFE,  // Least significant 3 bits is length_size_minus_one
      0xE1,  // Least significant 5 bits is num_sps
        // sps 1
        0x00, 0x1D,  // size
        0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
        0x00, 0x80, 0x00, 0x91, 0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
        0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x01,  // num_pps
        0x00, 0x06,  // size
        0x68, 0xEB, 0xE3, 0xCB, 0x22, 0xC0,
  };
  // clang-format on

  AVCDecoderConfigurationRecord avc_config;
  ASSERT_FALSE(avc_config.Parse(kAvcDecoderConfigurationData,
                                std::size(kAvcDecoderConfigurationData)));
}

TEST(AVCDecoderConfigurationRecordTest, FailOnInsufficientData) {
  const uint8_t kAvcDecoderConfigurationData[] = {0x01, 0x64, 0x00, 0x1E};

  AVCDecoderConfigurationRecord avc_config;
  ASSERT_FALSE(avc_config.Parse(kAvcDecoderConfigurationData,
                                std::size(kAvcDecoderConfigurationData)));
}

TEST(AVCDecoderConfigurationRecordTest, GetCodecString) {
  EXPECT_EQ("avc1.123456", AVCDecoderConfigurationRecord::GetCodecString(
                               FOURCC_avc1, 0x12, 0x34, 0x56));
}

}  // namespace media
}  // namespace shaka
