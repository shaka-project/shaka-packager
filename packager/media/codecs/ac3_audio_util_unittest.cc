// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/codecs/ac3_audio_util.h"

namespace shaka {
namespace media {

TEST(Ac3AudioUtilTest, ChannelTest1) {
  // audio_coding_mode is 7, which is Left, Center, Right, Left surround, Right
  // surround. LFE channel on.
  const std::vector<uint8_t> ac3_data = {0x10, 0x3d, 0xc0};

  EXPECT_EQ(6u, GetAc3NumChannels(ac3_data));
}

TEST(Ac3AudioUtilTest, ChannelTest2) {
  // audio_coding_mode is 2, which is Left and Right. LFE channel off.
  const std::vector<uint8_t> ac3_data = {0x10, 0x11, 0xc0};

  EXPECT_EQ(2u, GetAc3NumChannels(ac3_data));
}

}  // namespace media
}  // namespace shaka
