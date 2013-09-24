// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/aac.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace mp4 {

TEST(AACTest, BasicProfileTest) {
  AAC aac;
  uint8 buffer[] = {0x12, 0x10};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac.Parse(data));
  EXPECT_EQ(aac.GetOutputSamplesPerSecond(false), 44100);
  EXPECT_EQ(aac.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);
}

TEST(AACTest, ExtensionTest) {
  AAC aac;
  uint8 buffer[] = {0x13, 0x08, 0x56, 0xe5, 0x9d, 0x48, 0x80};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac.Parse(data));
  EXPECT_EQ(aac.GetOutputSamplesPerSecond(false), 48000);
  EXPECT_EQ(aac.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);
}

// Test implicit SBR with mono channel config.
// Mono channel layout should only be reported if SBR is not
// specified. Otherwise stereo should be reported.
// See ISO-14496-3 Section 1.6.6.1.2 for details about this special casing.
TEST(AACTest, ImplicitSBR_ChannelConfig0) {
  AAC aac;
  uint8 buffer[] = {0x13, 0x08};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac.Parse(data));

  // Test w/o implict SBR.
  EXPECT_EQ(aac.GetOutputSamplesPerSecond(false), 24000);
  EXPECT_EQ(aac.GetChannelLayout(false), CHANNEL_LAYOUT_MONO);

  // Test implicit SBR.
  EXPECT_EQ(aac.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac.GetChannelLayout(true), CHANNEL_LAYOUT_STEREO);
}

// Tests implicit SBR with a stereo channel config.
TEST(AACTest, ImplicitSBR_ChannelConfig1) {
  AAC aac;
  uint8 buffer[] = {0x13, 0x10};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac.Parse(data));

  // Test w/o implict SBR.
  EXPECT_EQ(aac.GetOutputSamplesPerSecond(false), 24000);
  EXPECT_EQ(aac.GetChannelLayout(false), CHANNEL_LAYOUT_STEREO);

  // Test implicit SBR.
  EXPECT_EQ(aac.GetOutputSamplesPerSecond(true), 48000);
  EXPECT_EQ(aac.GetChannelLayout(true), CHANNEL_LAYOUT_STEREO);
}

TEST(AACTest, SixChannelTest) {
  AAC aac;
  uint8 buffer[] = {0x11, 0xb0};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_TRUE(aac.Parse(data));
  EXPECT_EQ(aac.GetOutputSamplesPerSecond(false), 48000);
  EXPECT_EQ(aac.GetChannelLayout(false), CHANNEL_LAYOUT_5_1);
}

TEST(AACTest, DataTooShortTest) {
  AAC aac;
  std::vector<uint8> data;

  EXPECT_FALSE(aac.Parse(data));

  data.push_back(0x12);
  EXPECT_FALSE(aac.Parse(data));
}

TEST(AACTest, IncorrectProfileTest) {
  AAC aac;
  uint8 buffer[] = {0x0, 0x08};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_FALSE(aac.Parse(data));

  data[0] = 0x08;
  EXPECT_TRUE(aac.Parse(data));

  data[0] = 0x28;
  EXPECT_FALSE(aac.Parse(data));
}

TEST(AACTest, IncorrectFrequencyTest) {
  AAC aac;
  uint8 buffer[] = {0x0f, 0x88};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_FALSE(aac.Parse(data));

  data[0] = 0x0e;
  data[1] = 0x08;
  EXPECT_TRUE(aac.Parse(data));
}

TEST(AACTest, IncorrectChannelTest) {
  AAC aac;
  uint8 buffer[] = {0x0e, 0x00};
  std::vector<uint8> data;

  data.assign(buffer, buffer + sizeof(buffer));

  EXPECT_FALSE(aac.Parse(data));

  data[1] = 0x08;
  EXPECT_TRUE(aac.Parse(data));
}

}  // namespace mp4

}  // namespace media
