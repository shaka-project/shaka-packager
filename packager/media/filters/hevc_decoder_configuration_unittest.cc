// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/filters/hevc_decoder_configuration.h"

#include <gtest/gtest.h>

namespace edash_packager {
namespace media {

TEST(HEVCDecoderConfigurationTest, Success) {
  const uint8_t kHevcDecoderConfigurationData[] = {
      0x01, 0x02, 0x20, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3F, 0xF0, 0x00, 0xFC, 0xFD, 0xFA, 0xFA, 0x00, 0x00, 0x0F, 0x04, 0x20,
      0x00, 0x01, 0x00, 0x18, 0x40, 0x01, 0x0C, 0x01, 0xFF, 0xFF, 0x02, 0x20,
      0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
      0x3F, 0x99, 0x98, 0x09, 0x21, 0x00, 0x01, 0x00, 0x29, 0x42, 0x01, 0x01,
      0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00,
  };

  HEVCDecoderConfiguration hevc_config;
  ASSERT_TRUE(hevc_config.Parse(
      std::vector<uint8_t>(kHevcDecoderConfigurationData,
                           kHevcDecoderConfigurationData +
                               arraysize(kHevcDecoderConfigurationData))));

  EXPECT_EQ(4u, hevc_config.length_size());

  EXPECT_EQ("hev1.2.4.L63.90", hevc_config.GetCodecString(kCodecHEV1));
  EXPECT_EQ("hvc1.2.4.L63.90", hevc_config.GetCodecString(kCodecHVC1));
}

TEST(HEVCDecoderConfigurationTest, FailOnInsufficientData) {
  const uint8_t kHevcDecoderConfigurationData[] = {0x01, 0x02, 0x20, 0x00};

  HEVCDecoderConfiguration hevc_config;
  ASSERT_FALSE(hevc_config.Parse(
      std::vector<uint8_t>(kHevcDecoderConfigurationData,
                           kHevcDecoderConfigurationData +
                               arraysize(kHevcDecoderConfigurationData))));
}

}  // namespace media
}  // namespace edash_packager
