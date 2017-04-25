// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/vp_codec_configuration_record.h"

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(VPCodecConfigurationRecordTest, Parse) {
  const uint8_t kVpCodecConfigurationData[] = {
      0x01, 0x14, 0xA2, 0x02, 0x03, 0x04, 0x00, 0x00,
  };

  VPCodecConfigurationRecord vp_config;
  ASSERT_TRUE(vp_config.ParseMP4(
      std::vector<uint8_t>(std::begin(kVpCodecConfigurationData),
                           std::end(kVpCodecConfigurationData))));

  EXPECT_EQ(1u, vp_config.profile());
  EXPECT_EQ(20u, vp_config.level());
  EXPECT_EQ(10u, vp_config.bit_depth());
  EXPECT_EQ(1u, vp_config.chroma_subsampling());
  EXPECT_FALSE(vp_config.video_full_range_flag());
  EXPECT_EQ(2u, vp_config.color_primaries());
  EXPECT_EQ(3u, vp_config.transfer_characteristics());
  EXPECT_EQ(4u, vp_config.matrix_coefficients());

  EXPECT_EQ("vp09.01.20.10.01.02.03.04.00",
            vp_config.GetCodecString(kCodecVP9));
}

TEST(VPCodecConfigurationRecordTest, ParseWithInsufficientData) {
  const uint8_t kVpCodecConfigurationData[] = {
      0x01, 0x14, 0xA2, 0x02,
  };

  VPCodecConfigurationRecord vp_config;
  ASSERT_FALSE(vp_config.ParseMP4(
      std::vector<uint8_t>(std::begin(kVpCodecConfigurationData),
                           std::end(kVpCodecConfigurationData))));
}

TEST(VPCodecConfigurationRecordTest, WriteMP4) {
  const uint8_t kExpectedVpCodecConfigurationData[] = {
      0x02, 0x01, 0x85, 0x03, 0x04, 0x05, 0x00, 0x00,
  };
  VPCodecConfigurationRecord vp_config(0x02, 0x01, 0x08, 0x02, true, 0x03, 0x04,
                                       0x05, std::vector<uint8_t>());
  std::vector<uint8_t> data;
  vp_config.WriteMP4(&data);

  EXPECT_EQ(std::vector<uint8_t>(std::begin(kExpectedVpCodecConfigurationData),
                                 std::end(kExpectedVpCodecConfigurationData)),
            data);
}

TEST(VPCodecConfigurationRecordTest, WriteWebM) {
  const uint8_t kExpectedVpCodecConfigurationData[] = {
      0x01, 0x01, 0x02,
      0x02, 0x01, 0x01,
      0x03, 0x01, 0x08,
      0x04, 0x01, 0x02,
  };
  VPCodecConfigurationRecord vp_config(0x02, 0x01, 0x08, 0x02, true, 0x03, 0x04,
                                       0x05, std::vector<uint8_t>());
  std::vector<uint8_t> data;
  vp_config.WriteWebM(&data);

  EXPECT_EQ(std::vector<uint8_t>(std::begin(kExpectedVpCodecConfigurationData),
                                 std::end(kExpectedVpCodecConfigurationData)),
            data);
}

TEST(VPCodecConfigurationRecordTest, SetAttributes) {
  VPCodecConfigurationRecord vp_config;
  // None of the members are set.
  EXPECT_FALSE(vp_config.is_profile_set());
  EXPECT_FALSE(vp_config.is_level_set());
  EXPECT_FALSE(vp_config.is_bit_depth_set());
  EXPECT_FALSE(vp_config.is_chroma_subsampling_set());
  EXPECT_FALSE(vp_config.is_video_full_range_flag_set());
  EXPECT_FALSE(vp_config.is_color_primaries_set());
  EXPECT_FALSE(vp_config.is_transfer_characteristics_set());
  EXPECT_FALSE(vp_config.is_matrix_coefficients_set());

  const uint8_t kProfile = 2;
  vp_config.set_profile(kProfile);
  EXPECT_TRUE(vp_config.is_profile_set());
  EXPECT_EQ(kProfile, vp_config.profile());
}

TEST(VPCodecConfigurationRecordTest, SetChromaSubsampling) {
  VPCodecConfigurationRecord vp_config;
  vp_config.SetChromaSubsampling(1, 1);
  EXPECT_TRUE(vp_config.is_chroma_subsampling_set());
  EXPECT_FALSE(vp_config.is_chroma_location_set());
  EXPECT_EQ(VPCodecConfigurationRecord::CHROMA_420_COLLOCATED_WITH_LUMA,
            vp_config.chroma_subsampling());

  vp_config.SetChromaLocation(VPCodecConfigurationRecord::kLeftCollocated,
                              VPCodecConfigurationRecord::kHalf);
  EXPECT_TRUE(vp_config.is_chroma_location_set());
  EXPECT_EQ(VPCodecConfigurationRecord::CHROMA_420_VERTICAL,
            vp_config.chroma_subsampling());
}

TEST(VPCodecConfigurationRecordTest, Merge) {
  const uint8_t kProfile = 2;
  const uint8_t kLevel = 20;

  VPCodecConfigurationRecord vp_config;
  vp_config.set_profile(kProfile);

  VPCodecConfigurationRecord vp_config2;
  vp_config2.set_profile(kProfile - 1);
  vp_config2.set_level(kLevel);

  vp_config.MergeFrom(vp_config2);
  EXPECT_TRUE(vp_config.is_profile_set());
  EXPECT_TRUE(vp_config.is_level_set());
  EXPECT_FALSE(vp_config.is_bit_depth_set());
  EXPECT_FALSE(vp_config.is_chroma_subsampling_set());
  EXPECT_FALSE(vp_config.is_video_full_range_flag_set());
  EXPECT_FALSE(vp_config.is_color_primaries_set());
  EXPECT_FALSE(vp_config.is_transfer_characteristics_set());
  EXPECT_FALSE(vp_config.is_matrix_coefficients_set());

  // Profile is set in the original config, so not changed.
  EXPECT_EQ(kProfile, vp_config.profile());
  // Merge level from the other config.
  EXPECT_EQ(kLevel, vp_config.level());
}

TEST(VPCodecConfigurationRecordTest, MergeChromaSubsampling) {
  VPCodecConfigurationRecord vp_config;
  vp_config.SetChromaSubsampling(
      VPCodecConfigurationRecord::CHROMA_420_VERTICAL);

  VPCodecConfigurationRecord vp_config2;
  vp_config2.SetChromaLocation(VPCodecConfigurationRecord::kLeftCollocated,
                               VPCodecConfigurationRecord::kTopCollocated);

  vp_config.MergeFrom(vp_config2);
  EXPECT_FALSE(vp_config.is_profile_set());
  EXPECT_FALSE(vp_config.is_level_set());
  EXPECT_FALSE(vp_config.is_bit_depth_set());
  EXPECT_TRUE(vp_config.is_chroma_subsampling_set());
  EXPECT_TRUE(vp_config.is_chroma_location_set());
  EXPECT_FALSE(vp_config.is_video_full_range_flag_set());
  EXPECT_FALSE(vp_config.is_color_primaries_set());
  EXPECT_FALSE(vp_config.is_transfer_characteristics_set());
  EXPECT_FALSE(vp_config.is_matrix_coefficients_set());

  EXPECT_EQ(VPCodecConfigurationRecord::CHROMA_420_COLLOCATED_WITH_LUMA,
            vp_config.chroma_subsampling());
  EXPECT_EQ(AVCHROMA_LOC_TOPLEFT, vp_config.chroma_location());
}

}  // namespace media
}  // namespace shaka
