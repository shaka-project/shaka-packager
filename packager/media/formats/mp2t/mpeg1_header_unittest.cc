// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp2t/mpeg1_header.h>

#include <absl/log/log.h>
#include <absl/strings/numbers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/utils/hex_parser.h>

using ::testing::ElementsAreArray;

namespace {

const char kValidMp3SyncByte[] = "FFFD800444333332114322";

const char kInvalidMp3SyncByte_1[] = "F00150802EFFFB80044433";

const char kInvalidMp3SyncByte_2[] = "FF8050802EDF";

const char kInvalidMp3SyncByte_3[] = "FF8050802EDFFF";

const char kValidMp3Frame[] = "ffFD800444333332114322";

const char kInvalidMp3FrameBadVersion[] = "FFE8800444333332114322";

const char kInvalidMp3FrameBadLayer[] = "FFF9800444333332114322";

const char kInvalidMp3FrameBadBitrate[] = "FFFD000444333332114322";

const char kInvalidMp3FrameBadSamepleRate[] = "FFFD8C0444333332114322";

}  // anonymous namespace

namespace shaka {
namespace media {
namespace mp2t {

class Mpeg1HeaderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(shaka::ValidHexStringToBytes(kValidMp3SyncByte, &sync_valid_));
    ASSERT_TRUE(
        shaka::ValidHexStringToBytes(kInvalidMp3SyncByte_1, &sync_inv_1_));
    ASSERT_TRUE(
        shaka::ValidHexStringToBytes(kInvalidMp3SyncByte_2, &sync_inv_2_));
    ASSERT_TRUE(
        shaka::ValidHexStringToBytes(kInvalidMp3SyncByte_3, &sync_inv_3_));

    ASSERT_TRUE(shaka::ValidHexStringToBytes(kValidMp3Frame, &frame_valid_));
    ASSERT_TRUE(shaka::ValidHexStringToBytes(kInvalidMp3FrameBadVersion,
                                             &frame_inv_1_));
    ASSERT_TRUE(
        shaka::ValidHexStringToBytes(kInvalidMp3FrameBadLayer, &frame_inv_2_));
    ASSERT_TRUE(shaka::ValidHexStringToBytes(kInvalidMp3FrameBadBitrate,
                                             &frame_inv_3_));
    ASSERT_TRUE(shaka::ValidHexStringToBytes(kInvalidMp3FrameBadSamepleRate,
                                             &frame_inv_4_));
  }

 protected:
  std::vector<uint8_t> sync_valid_;
  std::vector<uint8_t> sync_inv_1_;
  std::vector<uint8_t> sync_inv_2_;
  std::vector<uint8_t> sync_inv_3_;

  std::vector<uint8_t> frame_valid_;
  std::vector<uint8_t> frame_inv_1_;
  std::vector<uint8_t> frame_inv_2_;
  std::vector<uint8_t> frame_inv_3_;
  std::vector<uint8_t> frame_inv_4_;
};

TEST_F(Mpeg1HeaderTest, SyncBytes) {
  Mpeg1Header mpeg1_header;

  ASSERT_TRUE(mpeg1_header.IsSyncWord(sync_valid_.data()));

  ASSERT_FALSE(mpeg1_header.IsSyncWord(sync_inv_1_.data()));
  ASSERT_FALSE(mpeg1_header.IsSyncWord(sync_inv_2_.data()));
  ASSERT_FALSE(mpeg1_header.IsSyncWord(sync_inv_3_.data()));
}

TEST_F(Mpeg1HeaderTest, Parsing) {
  Mpeg1Header mpeg1_header;

  // Success parsing
  EXPECT_EQ(static_cast<size_t>(417),
            mpeg1_header.GetFrameSizeWithoutParsing(frame_valid_.data(),
                                                    frame_valid_.size()));
  EXPECT_TRUE(mpeg1_header.Parse(frame_valid_.data(), frame_valid_.size()));
  EXPECT_EQ(static_cast<size_t>(417), mpeg1_header.GetFrameSize());
  EXPECT_EQ(static_cast<size_t>(44100), mpeg1_header.GetSamplingFrequency());
  EXPECT_EQ(static_cast<size_t>(1152), mpeg1_header.GetSamplesPerFrame());
  EXPECT_EQ(2, mpeg1_header.GetNumChannels());

  // Failed parsing
  EXPECT_FALSE(mpeg1_header.Parse(frame_inv_1_.data(), frame_inv_1_.size()));
  EXPECT_FALSE(mpeg1_header.Parse(frame_inv_2_.data(), frame_inv_2_.size()));
  EXPECT_FALSE(mpeg1_header.Parse(frame_inv_3_.data(), frame_inv_3_.size()));
  EXPECT_FALSE(mpeg1_header.Parse(frame_inv_4_.data(), frame_inv_4_.size()));
}

}  // Namespace mp2t
}  // namespace media
}  // namespace shaka
