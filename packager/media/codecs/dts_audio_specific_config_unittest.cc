// Copyright 2023 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "packager/media/codecs/dts_audio_specific_config.h"

namespace shaka {
namespace media {

TEST(DTSAudioSpecificConfigTest, BasicProfileTest) {
  uint8_t buffer[] = {0x01, 0x20, 0x00, 0x00, 0x0, 0x3F, 0x80, 0x00};
  std::vector<uint8_t> data(std::begin(buffer), std::end(buffer));
  uint32_t mask;
  EXPECT_TRUE(GetDTSXChannelMask(data, mask));
  EXPECT_EQ(0x3F, mask);
}

TEST(DTSAudioSpecificConfigTest, ChannelMaskBytes) {
  uint8_t buffer[] = {0x01, 0x20, 0x12, 0x34, 0x56, 0x78, 0x80, 0x00};
  std::vector<uint8_t> data(std::begin(buffer), std::end(buffer));
  uint32_t mask;
  EXPECT_TRUE(GetDTSXChannelMask(data, mask));
  EXPECT_EQ(0x12345678, mask);
}

TEST(DTSAudioSpecificConfigTest, Truncated) {
  uint8_t buffer[] = {0x01, 0x20, 0x00, 0x00, 0x00};
  std::vector<uint8_t> data(std::begin(buffer), std::end(buffer));
  uint32_t mask;
  EXPECT_FALSE(GetDTSXChannelMask(data, mask));
}

}  // namespace media
}  // namespace shaka
