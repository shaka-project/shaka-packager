// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/formats/mp2t/ac3_header.h"

namespace {

const char kValidPartialAc3Frame[] =
    "0B772770554043E106F575F080821010415C7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF"
    "9F3E7CF9F3EFF9D5F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3"
    "E7CF9F3E7CF9F3E7CF9F3E7CF9F3E3FE757CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F"
    "3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF8CBFC4912248000000000F1B6DB"
    "6DB6DE3C78F1DDDDDDDC00000000000000000000000000EEEEEEF1E3C6DB6DB6DB7CF9AD6B"
    "5AD6B5AD6B5AD6B5AD6B5AD6B4000000078DB6DB6DB6F1E3C78EEEEEEEE000000000000000"
    "0000000000077777778F1E36DB6DB6DBE7CD6B5AD6B5AD6B5AD6B5AD6B5AD6B5A600000000"
    "0003C6DB6DB6DB78F1E3C77777777000000000000000000000000003BBBBBBC78F1B6DB6DB"
    "6DF3E6B5AD6B5AD6B5AD6B5AD6B5AD6B5AD00000001E36DB6DB6DBC78F1E3BBBBBBB800000"
    "000000000000000000001DDDDDDE3C78DB6DB6DB6F9F35AD6B5AD6B5AD6B5AD6B5AD6B5AD6"
    "9800000000000F1B6DB6DB6DE3C78F1DDD";

const char kValidPartialAc3FrameSixChannels[] =
    "0B77A3B35E40EBF8403EFF9DF0C3F8430FE1FC155755DF3E7CFA33E7CF9F3E7CF9F3E7CF9F"
    "3ECDFF3ABE7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7CF9F3E7"
    "CF9F3E7CF9F3E7CF9F3E7C31F3E7CF9F3E7C7FCEAF9F3E7CF9F3";

}  // anonymous namespace

namespace shaka {
namespace media {
namespace mp2t {

class Ac3HeaderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(base::HexStringToBytes(kValidPartialAc3Frame, &ac3_frame_));
    ASSERT_TRUE(base::HexStringToBytes(kValidPartialAc3FrameSixChannels,
                                       &ac3_frame_six_channels_));
  }

 protected:
  std::vector<uint8_t> ac3_frame_;
  std::vector<uint8_t> ac3_frame_six_channels_;
};

TEST_F(Ac3HeaderTest, ParseSuccess) {
  const size_t kExpectedFrameSize(836);
  const size_t kExpectedHeaderSize(0);
  const uint8_t kExpectedObjectType(0);
  const uint32_t kExpectedSamplingFrequency(44100);
  const uint8_t kExpectedNumChannels(2);
  const uint8_t kExpectedAudioSpecificConfig[] = {0x50, 0x11, 0x40};

  Ac3Header ac3_header;
  ASSERT_TRUE(ac3_header.Parse(ac3_frame_.data(), ac3_frame_.size()));
  EXPECT_EQ(kExpectedFrameSize, ac3_header.GetFrameSize());
  EXPECT_EQ(kExpectedHeaderSize, ac3_header.GetHeaderSize());
  EXPECT_EQ(kExpectedObjectType, ac3_header.GetObjectType());
  EXPECT_EQ(kExpectedSamplingFrequency, ac3_header.GetSamplingFrequency());
  EXPECT_EQ(kExpectedNumChannels, ac3_header.GetNumChannels());
  std::vector<uint8_t> audio_specific_config;
  ac3_header.GetAudioSpecificConfig(&audio_specific_config);
  EXPECT_EQ(arraysize(kExpectedAudioSpecificConfig),
            audio_specific_config.size());
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kExpectedAudioSpecificConfig),
                                 std::end(kExpectedAudioSpecificConfig)),
            audio_specific_config);
}

TEST_F(Ac3HeaderTest, ParseMultiChannelSuccess) {
  const size_t kExpectedFrameSize(1950);
  const size_t kExpectedHeaderSize(0);
  const uint8_t kExpectedObjectType(0);
  const uint32_t kExpectedSamplingFrequency(44100);
  const uint8_t kExpectedNumChannels(6);
  const uint8_t kExpectedAudioSpecificConfig[] = {0x50, 0x3D, 0xE0};

  Ac3Header ac3_header;
  ASSERT_TRUE(ac3_header.Parse(ac3_frame_six_channels_.data(),
                               ac3_frame_six_channels_.size()));
  EXPECT_EQ(kExpectedFrameSize, ac3_header.GetFrameSize());
  EXPECT_EQ(kExpectedHeaderSize, ac3_header.GetHeaderSize());
  EXPECT_EQ(kExpectedObjectType, ac3_header.GetObjectType());
  EXPECT_EQ(kExpectedSamplingFrequency, ac3_header.GetSamplingFrequency());
  EXPECT_EQ(kExpectedNumChannels, ac3_header.GetNumChannels());
  std::vector<uint8_t> audio_specific_config;
  ac3_header.GetAudioSpecificConfig(&audio_specific_config);
  EXPECT_EQ(arraysize(kExpectedAudioSpecificConfig),
            audio_specific_config.size());
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kExpectedAudioSpecificConfig),
                                 std::end(kExpectedAudioSpecificConfig)),
            audio_specific_config);
}

TEST_F(Ac3HeaderTest, ParseVariousDataSize) {
  Ac3Header ac3_header;

  // Parse succeeds as long as the full metadata is provided.
  EXPECT_TRUE(ac3_header.Parse(ac3_frame_.data(), ac3_frame_.size() - 1));
  const size_t frame_size = ac3_header.GetFrameSize();
  const size_t header_size = ac3_header.GetHeaderSize();

  EXPECT_TRUE(ac3_header.Parse(ac3_frame_.data(), 100));
  EXPECT_EQ(frame_size, ac3_header.GetFrameSize());
  EXPECT_EQ(header_size, ac3_header.GetHeaderSize());

  // Parse fails if there is not enough data (no full metadata).
  EXPECT_FALSE(ac3_header.Parse(ac3_frame_.data(), 1));
  EXPECT_FALSE(ac3_header.Parse(ac3_frame_.data(), 5));
}

}  // Namespace mp2t
}  // namespace media
}  // namespace shaka
