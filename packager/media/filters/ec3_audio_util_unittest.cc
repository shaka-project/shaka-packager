// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/macros.h"
#include "packager/media/filters/ec3_audio_util.h"

namespace shaka {
namespace media {

TEST(EC3AudioUtilTest, CalculateEC3ChannelMapTest1) {
  // audio_coding_mode is 7, which is Left, Center, Right, Left surround, Right
  // surround. No dependent substreams. LFE channel on.
  const uint8_t kEc3Data[] = {0, 0, 0, 0x0f, 0};
  uint32_t channel_map;
  EXPECT_TRUE(CalculateEC3ChannelMap(
      std::vector<uint8_t>(kEc3Data, kEc3Data + arraysize(kEc3Data)),
      &channel_map));
  EXPECT_EQ(0xF801u, channel_map);
}

TEST(EC3AudioUtilTest, CalculateEC3ChannelMapTest2) {
  // audio_coding_mode is 2, which is Left and Right. No dependent substreams.
  // LFE channel off.
  const uint8_t kEc3Data[] = {0, 0, 0, 0x04, 0};
  uint32_t channel_map;
  EXPECT_TRUE(CalculateEC3ChannelMap(
      std::vector<uint8_t>(kEc3Data, kEc3Data + arraysize(kEc3Data)),
      &channel_map));
  EXPECT_EQ(0xA000u, channel_map);
}

TEST(EC3AudioUtilTest, CalculateEC3ChannelMapTest3) {
  // audio_coding_mode is 3, which is Left, Center, and Right. Dependent
  // substreams layout is 0b100000011, which is Left center/ Right center pair,
  // Left rear surround/ Right rear surround pair, LFE2 on. LFE channel on.
  const uint8_t kEc3Data[] = {0, 0, 0, 0x07, 0x07, 0x03};
  uint32_t channel_map;
  EXPECT_TRUE(CalculateEC3ChannelMap(
      std::vector<uint8_t>(kEc3Data, kEc3Data + arraysize(kEc3Data)),
      &channel_map));
  EXPECT_EQ(0xE603u, channel_map);
}

}  // namespace media
}  // namespace shaka
