// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/ac4_audio_util.h>

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(AC4AudioUtilTest, ChannelTest1) {
  // AC4 IMS
  const std::vector<uint8_t> ac4_data = {0x20, 0xa4, 0x02, 0x40, 0x00, 0x00,
                                         0x00, 0x1f, 0xff, 0xff, 0xff, 0xe0,
                                         0x02, 0x12, 0xf8, 0x80, 0x00, 0x00,
                                         0x42, 0x00, 0x00, 0x02, 0x50, 0x10,
                                         0x00, 0x00, 0x03, 0x10, 0x99, 0x5b,
                                         0xa0, 0x40, 0x01, 0x12, 0xf8, 0x80,
                                         0x00, 0x00, 0x42, 0x00, 0x00, 0x02,
                                         0x50, 0x10, 0x00, 0x00, 0x03, 0x10,
                                         0x99, 0x5b, 0x80, 0x40};

  uint32_t ac4_channel_mask;
  uint32_t ac4_channel_mpeg_value;
  uint8_t ac4_codec_info;
  bool ac4_ims_flag;
  bool ac4_cbi_flag;

  EXPECT_TRUE(CalculateAC4ChannelMask(ac4_data, &ac4_channel_mask));
  EXPECT_EQ((uint32_t)0x1, ac4_channel_mask);
  EXPECT_TRUE(CalculateAC4ChannelMPEGValue(ac4_data, &ac4_channel_mpeg_value));
  EXPECT_EQ((uint32_t)0x2, ac4_channel_mpeg_value);
  EXPECT_TRUE(GetAc4CodecInfo(ac4_data, &ac4_codec_info));
  EXPECT_EQ(80u, ac4_codec_info);
  EXPECT_TRUE(GetAc4ImmersiveInfo(ac4_data, &ac4_ims_flag, &ac4_cbi_flag));
  EXPECT_TRUE(ac4_ims_flag);
  EXPECT_FALSE(ac4_cbi_flag);
}

TEST(AC4AudioUtilTest, ChannelTest2) {
  // AC4 5.1-channel
  const std::vector<uint8_t> ac4_data = {0x20, 0xa6, 0x01, 0x60, 0x00, 0x00,
                                         0x00, 0x1f, 0xff, 0xff, 0xff, 0xe0,
                                         0x01, 0x0e, 0xf9, 0x00, 0x00, 0x09,
                                         0x00, 0x00, 0x11, 0xca, 0x02, 0x00,
                                         0x00, 0x11, 0xc0, 0x80};

  uint32_t ac4_channel_mask;
  uint32_t ac4_channel_mpeg_value;
  uint8_t ac4_codec_info;
  bool ac4_ims_flag;
  bool ac4_cbi_flag;

  EXPECT_TRUE(CalculateAC4ChannelMask(ac4_data, &ac4_channel_mask));
  EXPECT_EQ((uint32_t)0x47, ac4_channel_mask);
  EXPECT_TRUE(CalculateAC4ChannelMPEGValue(ac4_data, &ac4_channel_mpeg_value));
  EXPECT_EQ((uint32_t)0x6, ac4_channel_mpeg_value);
  EXPECT_TRUE(GetAc4CodecInfo(ac4_data, &ac4_codec_info));
  EXPECT_EQ(73u, ac4_codec_info);
  EXPECT_TRUE(GetAc4ImmersiveInfo(ac4_data, &ac4_ims_flag, &ac4_cbi_flag));
  EXPECT_FALSE(ac4_ims_flag);
  EXPECT_FALSE(ac4_cbi_flag);
}

TEST(AC4AudioUtilTest, ChannelTest3) {
  // AC4 stereo
  const std::vector<uint8_t> ac4_data = {0x20, 0xa4, 0x01, 0x40, 0x00, 0x00,
                                         0x00, 0x1f, 0xff, 0xff, 0xff, 0xe0,
                                         0x01, 0x12, 0xf8, 0x00, 0x00, 0x08,
                                         0x40, 0x00, 0x00, 0x4a, 0x02, 0x00,
                                         0x00, 0x00, 0x62, 0x13, 0x2b, 0x70,
                                         0x00, 0x80};

  uint32_t ac4_channel_mask;
  uint32_t ac4_channel_mpeg_value;
  uint8_t ac4_codec_info;
  bool ac4_ims_flag;
  bool ac4_cbi_flag;

  EXPECT_TRUE(CalculateAC4ChannelMask(ac4_data, &ac4_channel_mask));
  EXPECT_EQ((uint32_t)0x1, ac4_channel_mask);
  EXPECT_TRUE(CalculateAC4ChannelMPEGValue(ac4_data, &ac4_channel_mpeg_value));
  EXPECT_EQ((uint32_t)0x2, ac4_channel_mpeg_value);
  EXPECT_TRUE(GetAc4CodecInfo(ac4_data, &ac4_codec_info));
  EXPECT_EQ(72u, ac4_codec_info);
  EXPECT_TRUE(GetAc4ImmersiveInfo(ac4_data, &ac4_ims_flag, &ac4_cbi_flag));
  EXPECT_FALSE(ac4_ims_flag);
  EXPECT_FALSE(ac4_cbi_flag);
}

TEST(AC4AudioUtilTest, ChannelTest4) {
  // AC4 CBI 5.1.2
  const std::vector<uint8_t> ac4_data = {0x20, 0xa0, 0x01, 0x60, 0x00, 0x00,
                                         0x00, 0x1f, 0xff, 0xff, 0xff, 0xe0,
                                         0x01, 0x15, 0x13, 0x80, 0x00, 0x00,
                                         0x58, 0x40, 0x00, 0x31, 0xfc, 0xa0,
                                         0x20, 0x00, 0x03, 0x1d, 0x40, 0x40,
                                         0x00, 0x00, 0x08, 0x00, 0xc0};

  uint32_t ac4_channel_mask;
  uint32_t ac4_channel_mpeg_value;
  uint8_t ac4_codec_info;
  bool ac4_ims_flag;
  bool ac4_cbi_flag;

  EXPECT_TRUE(CalculateAC4ChannelMask(ac4_data, &ac4_channel_mask));
  EXPECT_EQ((uint32_t)0xC7, ac4_channel_mask);
  EXPECT_TRUE(CalculateAC4ChannelMPEGValue(ac4_data, &ac4_channel_mpeg_value));
  EXPECT_EQ((uint32_t)0xFFFFFFFF, ac4_channel_mpeg_value);
  EXPECT_TRUE(GetAc4CodecInfo(ac4_data, &ac4_codec_info));
  EXPECT_EQ(75u, ac4_codec_info);
  EXPECT_TRUE(GetAc4ImmersiveInfo(ac4_data, &ac4_ims_flag, &ac4_cbi_flag));
  EXPECT_FALSE(ac4_ims_flag);
  EXPECT_TRUE(ac4_cbi_flag);
}

}  // namespace media
}  // namespace shaka
