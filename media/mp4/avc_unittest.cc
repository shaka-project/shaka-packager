// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/basictypes.h"
#include "media/base/stream_parser_buffer.h"
#include "media/mp4/avc.h"
#include "media/mp4/box_definitions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"

namespace media {
namespace mp4 {

static const uint8 kNALU1[] = { 0x01, 0x02, 0x03 };
static const uint8 kNALU2[] = { 0x04, 0x05, 0x06, 0x07 };
static const uint8 kExpected[] = {
  0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03,
  0x00, 0x00, 0x00, 0x01, 0x04, 0x05, 0x06, 0x07 };

static const uint8 kExpectedParamSets[] = {
  0x00, 0x00, 0x00, 0x01, 0x67, 0x12,
  0x00, 0x00, 0x00, 0x01, 0x67, 0x34,
  0x00, 0x00, 0x00, 0x01, 0x68, 0x56, 0x78};

class AVCConversionTest : public testing::TestWithParam<int> {
 protected:
  void MakeInputForLength(int length_size, std::vector<uint8>* buf) {
    buf->clear();
    for (int i = 1; i < length_size; i++)
      buf->push_back(0);
    buf->push_back(sizeof(kNALU1));
    buf->insert(buf->end(), kNALU1, kNALU1 + sizeof(kNALU1));

    for (int i = 1; i < length_size; i++)
      buf->push_back(0);
    buf->push_back(sizeof(kNALU2));
    buf->insert(buf->end(), kNALU2, kNALU2 + sizeof(kNALU2));
  }
};

TEST_P(AVCConversionTest, ParseCorrectly) {
  std::vector<uint8> buf;
  MakeInputForLength(GetParam(), &buf);
  EXPECT_TRUE(AVC::ConvertFrameToAnnexB(GetParam(), &buf));
  EXPECT_EQ(buf.size(), sizeof(kExpected));
  EXPECT_EQ(0, memcmp(kExpected, &buf[0], sizeof(kExpected)));
}

TEST_P(AVCConversionTest, ParsePartial) {
  std::vector<uint8> buf;
  MakeInputForLength(GetParam(), &buf);
  buf.pop_back();
  EXPECT_FALSE(AVC::ConvertFrameToAnnexB(GetParam(), &buf));
  // This tests a buffer ending in the middle of a NAL length. For length size
  // of one, this can't happen, so we skip that case.
  if (GetParam() != 1) {
    MakeInputForLength(GetParam(), &buf);
    buf.erase(buf.end() - (sizeof(kNALU2) + 1), buf.end());
    EXPECT_FALSE(AVC::ConvertFrameToAnnexB(GetParam(), &buf));
  }
}

TEST_P(AVCConversionTest, ParseEmpty) {
  std::vector<uint8> buf;
  EXPECT_TRUE(AVC::ConvertFrameToAnnexB(GetParam(), &buf));
  EXPECT_EQ(0u, buf.size());
}

INSTANTIATE_TEST_CASE_P(AVCConversionTestValues,
                        AVCConversionTest,
                        ::testing::Values(1, 2, 4));

TEST_F(AVCConversionTest, ConvertConfigToAnnexB) {
  AVCDecoderConfigurationRecord avc_config;
  avc_config.sps_list.resize(2);
  avc_config.sps_list[0].push_back(0x67);
  avc_config.sps_list[0].push_back(0x12);
  avc_config.sps_list[1].push_back(0x67);
  avc_config.sps_list[1].push_back(0x34);
  avc_config.pps_list.resize(1);
  avc_config.pps_list[0].push_back(0x68);
  avc_config.pps_list[0].push_back(0x56);
  avc_config.pps_list[0].push_back(0x78);

  std::vector<uint8> buf;
  EXPECT_TRUE(AVC::ConvertConfigToAnnexB(avc_config, &buf));
  EXPECT_EQ(0, memcmp(kExpectedParamSets, &buf[0],
                      sizeof(kExpectedParamSets)));
}

}  // namespace mp4
}  // namespace media
