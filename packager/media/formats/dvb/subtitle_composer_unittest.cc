// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/dvb/subtitle_composer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shaka {
namespace media {

namespace {

constexpr const int kNoBgColor = -1;

void CreateDefaultImage(SubtitleComposer* composer, uint16_t object_id) {
  auto* image = composer->GetObjectImage(object_id);
  EXPECT_TRUE(image->AddPixel(BitDepth::k8Bit, 1, true));
  image->NewRow(true);
}

}  // namespace

TEST(SubtitleComposerTest, PositionsSamples) {
  const uint8_t kRegionId = 3, kColorSpaceId = 9;
  const uint16_t kObjectId1 = 5, kObjectId2 = 11;
  SubtitleComposer composer;
  composer.SetDisplaySize(100, 100);
  ASSERT_TRUE(composer.SetRegionPosition(kRegionId, 4, 5));
  ASSERT_TRUE(composer.SetRegionInfo(kRegionId, kColorSpaceId, 96, 95));
  ASSERT_TRUE(composer.SetObjectInfo(kObjectId1, kRegionId, 8, 9, kNoBgColor));
  ASSERT_TRUE(
      composer.SetObjectInfo(kObjectId2, kRegionId, 12, 14, kNoBgColor));
  CreateDefaultImage(&composer, kObjectId1);
  CreateDefaultImage(&composer, kObjectId2);

  std::vector<std::shared_ptr<TextSample>> samples;
  ASSERT_TRUE(composer.GetSamples(0, 1, &samples));
  ASSERT_EQ(samples.size(), 2u);
  for (size_t i = 0; i < samples.size(); i++) {
    ASSERT_TRUE(samples[i]->settings().line);
    ASSERT_EQ(samples[i]->settings().line->type, TextUnitType::kPercent);
    ASSERT_TRUE(samples[i]->settings().position);
    ASSERT_EQ(samples[i]->settings().position->type, TextUnitType::kPercent);
    ASSERT_TRUE(samples[i]->settings().width);
    ASSERT_EQ(samples[i]->settings().width->type, TextUnitType::kPercent);
    ASSERT_EQ(samples[i]->settings().width->value, 1);
    ASSERT_TRUE(samples[i]->settings().height);
    ASSERT_EQ(samples[i]->settings().height->type, TextUnitType::kPercent);
    ASSERT_EQ(samples[i]->settings().height->value, 1);
    ASSERT_FALSE(samples[i]->body().image.empty());
  }
  // Allow in either order.
  if (samples[0]->settings().position->value == 12) {
    EXPECT_EQ(samples[0]->settings().line->value, 14);
    EXPECT_EQ(samples[1]->settings().position->value, 16);
    EXPECT_EQ(samples[1]->settings().line->value, 19);
  } else {
    EXPECT_EQ(samples[0]->settings().position->value, 16);
    EXPECT_EQ(samples[0]->settings().line->value, 19);
    EXPECT_EQ(samples[1]->settings().position->value, 12);
    EXPECT_EQ(samples[1]->settings().line->value, 14);
  }
}

TEST(SubtitleComposerTest, EnsuresRegionsFit) {
  SubtitleComposer composer;
  composer.SetDisplaySize(0xfff0, 0xfff0);
  ASSERT_FALSE(composer.SetRegionPosition(1, 0xffff, 0xffff));
  ASSERT_TRUE(composer.SetRegionPosition(1, 0xff00, 0xff00));
  ASSERT_FALSE(composer.SetRegionInfo(1, 0, 0xf1, 0));
  ASSERT_FALSE(composer.SetRegionInfo(1, 0, 0, 0xf1));
  ASSERT_FALSE(composer.SetRegionInfo(1, 0, 0xff1, 0));
  ASSERT_FALSE(composer.SetRegionInfo(1, 0, 0, 0xff1));
  ASSERT_TRUE(composer.SetRegionInfo(1, 0, 25, 25));

  ASSERT_FALSE(composer.SetObjectInfo(1, 1, 0xf1, 0, kNoBgColor));
  ASSERT_FALSE(composer.SetObjectInfo(1, 1, 0, 0xf1, kNoBgColor));
  ASSERT_FALSE(composer.SetObjectInfo(1, 1, 0xff1, 0, kNoBgColor));
  ASSERT_FALSE(composer.SetObjectInfo(1, 1, 0, 0xff1, kNoBgColor));
  ASSERT_TRUE(composer.SetObjectInfo(1, 1, 20, 20, kNoBgColor));
}

TEST(SubtitleComposerTest, MustInitRegionFirst) {
  SubtitleComposer composer;
  EXPECT_FALSE(composer.SetObjectInfo(0, 0, 0, 0, kNoBgColor));
}

TEST(SubtitleComposerTest, ReturnsConsistentColorSpace) {
  const uint8_t kColorSpaceId = 2;
  const uint16_t kObjectId = 5;
  const uint16_t kRegionId = 1;

  // Initially created in GetColorSpace.
  {
    SubtitleComposer composer;
    auto* color_space = composer.GetColorSpace(kColorSpaceId);
    ASSERT_TRUE(composer.SetRegionInfo(kRegionId, kColorSpaceId, 1, 1));
    ASSERT_TRUE(composer.SetObjectInfo(kObjectId, kRegionId, 0, 0, kNoBgColor));
    ASSERT_EQ(composer.GetColorSpace(kColorSpaceId), color_space);
    ASSERT_EQ(composer.GetColorSpaceForObject(kObjectId), color_space);
  }

  // Initially created in SetRegionInfo.
  {
    SubtitleComposer composer;
    ASSERT_TRUE(composer.SetRegionInfo(kRegionId, kColorSpaceId, 1, 1));
    ASSERT_TRUE(composer.SetObjectInfo(kObjectId, kRegionId, 0, 0, kNoBgColor));
    auto* color_space = composer.GetColorSpace(kColorSpaceId);
    ASSERT_EQ(composer.GetColorSpace(kColorSpaceId), color_space);
    ASSERT_EQ(composer.GetColorSpaceForObject(kObjectId), color_space);
  }
}

TEST(SubtitleComposerTest, ClearObjects) {
  const uint8_t kColorSpaceId = 2;
  const uint16_t kObjectId1 = 5;
  const uint16_t kObjectId2 = 6;
  const uint16_t kRegionId = 1;

  SubtitleComposer composer;
  ASSERT_TRUE(composer.SetRegionInfo(kRegionId, kColorSpaceId, 10, 10));
  ASSERT_TRUE(composer.SetObjectInfo(kObjectId1, kRegionId, 0, 0, kNoBgColor));
  CreateDefaultImage(&composer, kObjectId1);

  std::vector<std::shared_ptr<TextSample>> samples;
  ASSERT_TRUE(composer.GetSamples(0, 1, &samples));
  EXPECT_EQ(samples.size(), 1u);

  composer.ClearObjects();
  samples.clear();

  // Should clear regions too.
  ASSERT_FALSE(composer.SetObjectInfo(kObjectId2, kRegionId, 3, 3, kNoBgColor));
  ASSERT_TRUE(composer.SetRegionInfo(kRegionId, kColorSpaceId, 10, 10));
  ASSERT_TRUE(composer.SetObjectInfo(kObjectId2, kRegionId, 3, 3, kNoBgColor));
  CreateDefaultImage(&composer, kObjectId2);

  ASSERT_TRUE(composer.GetSamples(0, 1, &samples));
  EXPECT_EQ(samples.size(), 1u);
}

TEST(SubtitleComposerTest, IgnoresEmptyImages) {
  const uint8_t kColorSpaceId = 1;
  const uint16_t kRegionId = 1;
  const uint16_t kObjectId1 = 2;
  const uint16_t kObjectId2 = 3;
  const uint16_t kObjectId3 = 4;
  const uint8_t kColorId = 10;

  SubtitleComposer composer;
  ASSERT_TRUE(composer.SetRegionInfo(kRegionId, kColorSpaceId, 10, 10));
  ASSERT_TRUE(composer.SetObjectInfo(kObjectId1, kRegionId, 0, 0, kNoBgColor));
  ASSERT_TRUE(composer.SetObjectInfo(kObjectId2, kRegionId, 5, 0, kNoBgColor));
  ASSERT_TRUE(composer.SetObjectInfo(kObjectId3, kRegionId, 0, 5, kNoBgColor));
  // Leave kObjectId1 with nothing.
  CreateDefaultImage(&composer, kObjectId2);
  {
    // Add a transparent color.
    auto* color_space = composer.GetColorSpace(kColorSpaceId);
    color_space->SetColor(BitDepth::k8Bit, kColorId, RgbaColor{0, 0, 0, 0});

    auto* image = composer.GetObjectImage(kObjectId3);
    EXPECT_TRUE(image->AddPixel(BitDepth::k8Bit, kColorId, true));
    image->NewRow(true);
  }

  std::vector<std::shared_ptr<TextSample>> samples;
  ASSERT_TRUE(composer.GetSamples(0, 1, &samples));
  EXPECT_EQ(samples.size(), 1u);
}

}  // namespace media
}  // namespace shaka
