// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/dovi_decoder_configuration_record.h>

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(DOVIDecoderConfigurationRecordTest, Success) {
  const std::vector<uint8_t> dovi_config_data = {
      0x01,       // Major Version
      0x00,       // Minor Version
      0x05 << 1,  // Profile
      0x08 << 3,  // Level
      // Other data we do not care.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  DOVIDecoderConfigurationRecord dovi_config;
  ASSERT_TRUE(dovi_config.Parse(dovi_config_data));

  EXPECT_EQ("dvh1.05.08", dovi_config.GetCodecString(FOURCC_dvh1));
  EXPECT_EQ("dvhe.05.08", dovi_config.GetCodecString(FOURCC_dvhe));
}

TEST(DOVIDecoderConfigurationRecordTest, FailOnIncorectVersion) {
  const std::vector<uint8_t> dovi_config_data = {0x02, 0x00, 0x0A, 0x04};

  DOVIDecoderConfigurationRecord dovi_config;
  ASSERT_FALSE(dovi_config.Parse(dovi_config_data));
}

TEST(DOVIDecoderConfigurationRecordTest, FailOnInsufficientData) {
  const std::vector<uint8_t> dovi_config_data = {0x01, 0x00, 0x0A};

  DOVIDecoderConfigurationRecord dovi_config;
  ASSERT_FALSE(dovi_config.Parse(dovi_config_data));
}

}  // namespace media
}  // namespace shaka
