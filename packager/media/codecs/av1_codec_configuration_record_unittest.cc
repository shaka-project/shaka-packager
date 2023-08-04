// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/av1_codec_configuration_record.h"

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(AV1CodecConfigurationRecordTest, Success) {
  const uint8_t kAV1CodecConfigurationData[] = {
      0x81,  // mark bit and version
      0x04,  // profile = 0, level = 4
      0x4E,  // tier = 0, bit_depth = 10, mono_chrome = 0
             // chroma_subsampling_x = 1, chroma_subsampling_y = 1,
             // chroma_sample_position = 2
      // We do not care about other data.
      0x00,
  };

  AV1CodecConfigurationRecord av1_config;
  ASSERT_TRUE(av1_config.Parse(
      std::vector<uint8_t>(std::begin(kAV1CodecConfigurationData),
                           std::end(kAV1CodecConfigurationData))));

  EXPECT_EQ(av1_config.GetCodecString(), "av01.0.04M.10");
  EXPECT_EQ(av1_config.GetCodecString(10, 8, 4, 1),
            "av01.0.04M.10.0.112.10.08.04.1");
}

TEST(AV1CodecConfigurationRecordTest, Success2) {
  const uint8_t kAV1CodecConfigurationData[] = {
      0x81,  // mark bit and version
      0x35,  // profile = 1, level = 15
      0xF4,  // tier = 1, bit_depth = 12, mono_chrome = 1
             // chroma_subsampling_x = 0, chroma_subsampling_y = 1,
             // chroma_sample_position = 0
      // We do not care about other data.
      0x00,
  };

  AV1CodecConfigurationRecord av1_config;
  ASSERT_TRUE(av1_config.Parse(
      std::vector<uint8_t>(std::begin(kAV1CodecConfigurationData),
                           std::end(kAV1CodecConfigurationData))));

  EXPECT_EQ(av1_config.GetCodecString(), "av01.1.21H.12");
  EXPECT_EQ(av1_config.GetCodecString(1, 1, 1, 0),
            "av01.1.21H.12.1.010.01.01.01.0");
}

TEST(AV1CodecConfigurationRecordTest, InsufficientData) {
  const uint8_t kAV1CodecConfigurationData[] = {
      0x81,
      0x04,
  };

  AV1CodecConfigurationRecord av1_config;
  ASSERT_FALSE(av1_config.Parse(
      std::vector<uint8_t>(std::begin(kAV1CodecConfigurationData),
                           std::end(kAV1CodecConfigurationData))));
}

TEST(AV1CodecConfigurationRecordTest, IncorrectMarkerBit) {
  const uint8_t kAV1CodecConfigurationData[] = {
      0x01,
      0x04,
      0x4E,
  };

  AV1CodecConfigurationRecord av1_config;
  ASSERT_FALSE(av1_config.Parse(
      std::vector<uint8_t>(std::begin(kAV1CodecConfigurationData),
                           std::end(kAV1CodecConfigurationData))));
}

TEST(AV1CodecConfigurationRecordTest, IncorrectVersion) {
  const uint8_t kAV1CodecConfigurationData[] = {
      0x82,
      0x04,
      0x4E,
  };

  AV1CodecConfigurationRecord av1_config;
  ASSERT_FALSE(av1_config.Parse(
      std::vector<uint8_t>(std::begin(kAV1CodecConfigurationData),
                           std::end(kAV1CodecConfigurationData))));
}

}  // namespace media
}  // namespace shaka
