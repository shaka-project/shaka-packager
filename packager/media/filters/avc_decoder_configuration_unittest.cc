// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/avc_decoder_configuration.h"

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(AVCDecoderConfigurationTest, Success) {
  const uint8_t kAvcDecoderConfigurationData[] = {
      0x01, 0x64, 0x00, 0x1E, 0xFF, 0xE1, 0x00, 0x1D, 0x67, 0x64, 0x00, 0x1E,
      0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA, 0x60, 0x0F, 0x16, 0x2D,
      0x96, 0x01, 0x00, 0x06, 0x68, 0xEB, 0xE3, 0xCB, 0x22, 0xC0};

  AVCDecoderConfiguration avc_config;
  ASSERT_TRUE(avc_config.Parse(kAvcDecoderConfigurationData,
                               arraysize(kAvcDecoderConfigurationData)));

  EXPECT_EQ(1u, avc_config.version());
  EXPECT_EQ(0x64, avc_config.profile_indication());
  EXPECT_EQ(0u, avc_config.profile_compatibility());
  EXPECT_EQ(0x1E, avc_config.avc_level());
  EXPECT_EQ(4u, avc_config.nalu_length_size());
  EXPECT_EQ(720u, avc_config.coded_width());
  EXPECT_EQ(360u, avc_config.coded_height());
  EXPECT_EQ(8u, avc_config.pixel_width());
  EXPECT_EQ(9u, avc_config.pixel_height());

  EXPECT_EQ("avc1.64001e", avc_config.GetCodecString());
}

TEST(AVCDecoderConfigurationTest, FailsOnInvalidNaluLengthSize) {
  const uint8_t kAvcDecoderConfigurationData[] = {
      0x01, 0x64, 0x00, 0x1E, 0xFE, 0xE1, 0x00, 0x1D, 0x67, 0x64, 0x00, 0x1E,
      0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA, 0x60, 0x0F, 0x16, 0x2D,
      0x96, 0x01, 0x00, 0x06, 0x68, 0xEB, 0xE3, 0xCB, 0x22, 0xC0};

  AVCDecoderConfiguration avc_config;
  ASSERT_FALSE(avc_config.Parse(kAvcDecoderConfigurationData,
                                arraysize(kAvcDecoderConfigurationData)));
}

TEST(AVCDecoderConfigurationTest, FailOnInsufficientData) {
  const uint8_t kAvcDecoderConfigurationData[] = {0x01, 0x64, 0x00, 0x1E};

  AVCDecoderConfiguration avc_config;
  ASSERT_FALSE(avc_config.Parse(kAvcDecoderConfigurationData,
                                arraysize(kAvcDecoderConfigurationData)));
}

TEST(AVCDecoderConfigurationTest, GetCodecString) {
  EXPECT_EQ("avc1.123456",
            AVCDecoderConfiguration::GetCodecString(0x12, 0x34, 0x56));
}

}  // namespace media
}  // namespace shaka
