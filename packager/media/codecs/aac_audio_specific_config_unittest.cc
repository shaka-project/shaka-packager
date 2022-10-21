// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "packager/media/codecs/aac_audio_specific_config.h"

namespace shaka {
namespace media {

TEST(AACAudioSpecificConfigTest, BasicProfileTest) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = {0x12, 0x10};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac_audio_specific_config.Parse(data));
  EXPECT_EQ(44100u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(2u, aac_audio_specific_config.GetNumChannels());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_AAC_LC,
            aac_audio_specific_config.GetAudioObjectType());
}

TEST(AACAudioSpecificConfigTest, ExtensionTest) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = {0x13, 0x08, 0x56, 0xe5, 0x9d, 0x48, 0x80};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac_audio_specific_config.Parse(data));
  EXPECT_EQ(48000u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(2u, aac_audio_specific_config.GetNumChannels());
  EXPECT_TRUE(aac_audio_specific_config.sbr_present());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_PS,
            aac_audio_specific_config.GetAudioObjectType());
}

// Test implicit SBR with mono channel config.
// Mono channel layout should only be reported if SBR is not
// specified. Otherwise stereo should be reported.
// See ISO-14496-3 Section 1.6.6.1.2 for details about this special casing.
TEST(AACAudioSpecificConfigTest, ImplicitSBR_ChannelConfig0) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = {0x13, 0x08};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac_audio_specific_config.Parse(data));

  EXPECT_EQ(24000u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(1u, aac_audio_specific_config.GetNumChannels());
  EXPECT_FALSE(aac_audio_specific_config.sbr_present());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_AAC_LC,
            aac_audio_specific_config.GetAudioObjectType());

  aac_audio_specific_config.set_sbr_present(true);
  EXPECT_EQ(48000u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(2u, aac_audio_specific_config.GetNumChannels());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_SBR,
            aac_audio_specific_config.GetAudioObjectType());
}

// Tests implicit SBR with a stereo channel config.
TEST(AACAudioSpecificConfigTest, ImplicitSBR_ChannelConfig1) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = {0x13, 0x10};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac_audio_specific_config.Parse(data));

  EXPECT_EQ(24000u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(2u, aac_audio_specific_config.GetNumChannels());
  EXPECT_FALSE(aac_audio_specific_config.sbr_present());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_AAC_LC,
            aac_audio_specific_config.GetAudioObjectType());

  aac_audio_specific_config.set_sbr_present(true);
  EXPECT_EQ(48000u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(2u, aac_audio_specific_config.GetNumChannels());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_SBR,
            aac_audio_specific_config.GetAudioObjectType());
}

TEST(AACAudioSpecificConfigTest, SixChannelTest) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = {0x11, 0xb0};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac_audio_specific_config.Parse(data));
  EXPECT_EQ(48000u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(6u, aac_audio_specific_config.GetNumChannels());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_AAC_LC,
            aac_audio_specific_config.GetAudioObjectType());
}

TEST(AACAudioSpecificConfigTest, UsacTest) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = { 
    0xF9, 0x5E, 0x01, 0x2C, 0x00, 0x52, 0x42, 0x2C, 0xC0, 0x51,
    0x17, 0x55, 0x4F, 0x36, 0x00, 0x42, 0x80, 0x01, 0x00, 0x04,
    0xA8, 0x82, 0x34, 0xE5, 0x80
  };

  std::vector<uint8_t> data(std::begin(buffer), std::end(buffer));

  EXPECT_TRUE(aac_audio_specific_config.Parse(data));
  EXPECT_EQ(38400u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(2u, aac_audio_specific_config.GetNumChannels());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_USAC,
            aac_audio_specific_config.GetAudioObjectType());
}

TEST(AACAudioSpecificConfigTest, ProgramConfigElementTest) {
  uint8_t buffer[] = {
      0x11, 0x80, 0x04, 0xC8, 0x44, 0x00, 0x20, 0x00, 0xC4,
      0x0D, 0x4C, 0x61, 0x76, 0x63, 0x35, 0x38, 0x2E, 0x31,
      0x38, 0x2E, 0x31, 0x30, 0x30, 0x56, 0xE5, 0x00,
  };
  std::vector<uint8_t> data(std::begin(buffer), std::end(buffer));

  AACAudioSpecificConfig aac_audio_specific_config;
  EXPECT_TRUE(aac_audio_specific_config.Parse(data));
  EXPECT_EQ(48000u, aac_audio_specific_config.GetSamplesPerSecond());
  EXPECT_EQ(6u, aac_audio_specific_config.GetNumChannels());
  EXPECT_EQ(AACAudioSpecificConfig::AOT_AAC_LC,
            aac_audio_specific_config.GetAudioObjectType());
}

TEST(AACAudioSpecificConfigTest, DataTooShortTest) {
  AACAudioSpecificConfig aac_audio_specific_config;
  std::vector<uint8_t> data;

  EXPECT_FALSE(aac_audio_specific_config.Parse(data));

  data.push_back(0x12);
  EXPECT_FALSE(aac_audio_specific_config.Parse(data));
}

TEST(AACAudioSpecificConfigTest, IncorrectProfileTest) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = {0x0, 0x08};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_FALSE(aac_audio_specific_config.Parse(data));

  data[0] = 0x08;
  EXPECT_TRUE(aac_audio_specific_config.Parse(data));

  data[0] = 0x28;
  EXPECT_FALSE(aac_audio_specific_config.Parse(data));
}

TEST(AACAudioSpecificConfigTest, IncorrectFrequencyTest) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = {0x0f, 0x88};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_FALSE(aac_audio_specific_config.Parse(data));

  data[0] = 0x0e;
  data[1] = 0x08;
  EXPECT_TRUE(aac_audio_specific_config.Parse(data));
}

TEST(AACAudioSpecificConfigTest, IncorrectChannelTest) {
  AACAudioSpecificConfig aac_audio_specific_config;
  uint8_t buffer[] = {0x0e, 0x00};
  std::vector<uint8_t> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_FALSE(aac_audio_specific_config.Parse(data));

  data[1] = 0x08;
  EXPECT_TRUE(aac_audio_specific_config.Parse(data));
}

}  // namespace media
}  // namespace shaka
