// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/codecs/ec3_audio_util.h"

namespace shaka {
namespace media {

TEST(EC3AudioUtilTest, ChannelTest1) {
  // audio_coding_mode is 7, which is Left, Center, Right, Left surround, Right
  // surround. No dependent substreams. LFE channel on.
  const std::vector<uint8_t> ec3_data = {0, 0, 0, 0x0f, 0};

  uint32_t channel_map;
  EXPECT_TRUE(CalculateEC3ChannelMap(ec3_data, &channel_map));
  EXPECT_EQ(0xF801u, channel_map);
  EXPECT_EQ(6u, GetEc3NumChannels(ec3_data));
}

TEST(EC3AudioUtilTest, ChannelTest2) {
  // audio_coding_mode is 2, which is Left and Right. No dependent substreams.
  // LFE channel off.
  const std::vector<uint8_t> ec3_data = {0, 0, 0, 0x04, 0};

  uint32_t channel_map;
  EXPECT_TRUE(CalculateEC3ChannelMap(ec3_data, &channel_map));
  EXPECT_EQ(0xA000u, channel_map);
  EXPECT_EQ(2u, GetEc3NumChannels(ec3_data));
}

TEST(EC3AudioUtilTest, ChannelTest3) {
  // audio_coding_mode is 3, which is Left, Center, and Right. Dependent
  // substreams layout is 0b100000011, which is Left center/ Right center pair,
  // Left rear surround/ Right rear surround pair, LFE2 on. LFE channel on.
  const std::vector<uint8_t> ec3_data = {0, 0, 0, 0x07, 0x07, 0x03};

  uint32_t channel_map;
  EXPECT_TRUE(CalculateEC3ChannelMap(ec3_data, &channel_map));
  EXPECT_EQ(0xE603u, channel_map);
  EXPECT_EQ(9u, GetEc3NumChannels(ec3_data));
}

}  // namespace media
}  // namespace shaka
