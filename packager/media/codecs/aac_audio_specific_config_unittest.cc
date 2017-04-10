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
