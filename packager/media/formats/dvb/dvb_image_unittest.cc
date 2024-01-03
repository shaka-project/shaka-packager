// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/dvb/dvb_image.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/media/base/rcheck.h>

namespace shaka {
namespace media {

namespace {

// Use an unusual alpha value to avoid being equal to a default color.
const RgbaColor kRed{255, 0, 0, 211};
const RgbaColor kGreen{0, 255, 0, 211};
const RgbaColor kBlue{0, 0, 255, 211};
const RgbaColor kBlack{0, 0, 0, 211};
const RgbaColor kWhite{255, 255, 255, 211};
const RgbaColor kYellow{255, 255, 0, 211};
const uint8_t kRedId = 0;
const uint8_t kGreenId = 1;
const uint8_t kBlueId = 2;
const uint8_t kBlackId = 3;
const uint8_t kWhiteId = 4;
const uint8_t kYellowId = 5;

const bool kTopRow = true;
const bool kBottomRow = false;

void FillDefaultColorSpace(DvbImageColorSpace* space) {
  for (auto depth : {BitDepth::k2Bit, BitDepth::k4Bit, BitDepth::k8Bit}) {
    space->SetColor(depth, kRedId, kRed);
    space->SetColor(depth, kGreenId, kGreen);
    space->SetColor(depth, kBlueId, kBlue);
    space->SetColor(depth, kBlackId, kBlack);
    if (depth != BitDepth::k2Bit) {
      space->SetColor(depth, kWhiteId, kWhite);
      space->SetColor(depth, kYellowId, kYellow);
    }
  }
}

bool AddPixelRow(DvbImageBuilder* image,
                 uint16_t width,
                 uint8_t color_id,
                 bool is_top_rows) {
  for (size_t i = 0; i < width; i++)
    RCHECK(image->AddPixel(BitDepth::k8Bit, color_id, is_top_rows));
  image->NewRow(is_top_rows);
  return true;
}

void CheckImagePixels(const DvbImageBuilder* image,
                      uint16_t width,
                      std::initializer_list<RgbaColor> rows) {
  uint16_t actual_width, height;
  const RgbaColor* pixels = nullptr;
  ASSERT_TRUE(image->GetPixels(&pixels, &actual_width, &height));
  ASSERT_EQ(actual_width, width);
  ASSERT_EQ(height, rows.size());

  size_t row = 0;
  for (const auto& color : rows) {
    for (size_t i = 0; i < width; i++)
      EXPECT_EQ(pixels[image->max_width() * row + i], color);
    row++;
  }
}

}  // namespace

TEST(DvbImageColorSpaceTest, GetsColors) {
  DvbImageColorSpace space;
  space.SetColor(BitDepth::k8Bit, 0, kRed);
  space.SetColor(BitDepth::k8Bit, 1, kGreen);
  space.SetColor(BitDepth::k8Bit, 2, kBlue);

  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0), kRed);
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 1), kGreen);
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 2), kBlue);
}

TEST(DvbImageColorSpaceTest, BitDepthsAreDifferent) {
  DvbImageColorSpace space;
  space.SetColor(BitDepth::k8Bit, 0, kRed);
  space.SetColor(BitDepth::k8Bit, 1, kGreen);
  space.SetColor(BitDepth::k4Bit, 0, kBlue);
  space.SetColor(BitDepth::k4Bit, 1, kBlack);
  space.SetColor(BitDepth::k2Bit, 0, kWhite);
  space.SetColor(BitDepth::k2Bit, 1, kYellow);

  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0), kRed);
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 1), kGreen);
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0), kBlue);
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 1), kBlack);
  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0), kWhite);
  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 1), kYellow);
}

TEST(DvbImageColorSpaceTest, HandlesBitDepthReduction) {
  DvbImageColorSpace space;
  space.SetColor(BitDepth::k2Bit, 0x0, kRed);
  space.SetColor(BitDepth::k2Bit, 0x1, kGreen);
  space.SetColor(BitDepth::k4Bit, 0x1, kWhite);
  space.SetColor(BitDepth::k4Bit, 0x5, kBlue);
  space.SetColor(BitDepth::k4Bit, 0x7, kBlack);
  space.SetColor(BitDepth::k4Bit, 0x9, kYellow);

  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x00), kRed);     // 0x0 in 2-bit
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x02), kGreen);   // 0x1 in 2-bit
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x72), kGreen);   // 0x1 in 2-bit
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x35), kBlue);    // 0x5 in 4-bit
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x17), kBlack);   // 0x7 in 4-bit
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x09), kYellow);  // Exact match

  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x0), kRed);    // 0x0 in 2-bit
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x2), kGreen);  // 0x1 in 2-bit
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x3), kGreen);  // 0x1 in 2-bit
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x1), kWhite);  // Exact match
}

TEST(DvbImageColorSpaceTest, HandlesBitDepthExpansion) {
  DvbImageColorSpace space;
  space.SetColor(BitDepth::k2Bit, 0x0, kRed);
  space.SetColor(BitDepth::k4Bit, 0x7, kGreen);
  space.SetColor(BitDepth::k4Bit, 0x8, kBlue);
  space.SetColor(BitDepth::k8Bit, 0x11, kBlack);
  space.SetColor(BitDepth::k8Bit, 0xff, kYellow);

  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0x0), kRed);     // Exact match
  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0x1), kGreen);   // 0x07 in 4-bit
  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0x3), kYellow);  // 0xff in 8-bit
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x7), kGreen);   // Exact match
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x1), kBlack);   // 0x11 in 8-bit
}

TEST(DvbImageColorSpaceTest, HandlesCustomBitDepthExpansion) {
  const uint8_t k2To4Map[] = {0x0, 0x6, 0x7, 0x0};
  const uint8_t k2To8Map[] = {0x0, 0xa, 0xb, 0x0};
  const uint8_t k4To8Map[] = {0x0, 0x12, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
                              0x0, 0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
  DvbImageColorSpace space;
  space.Set2To4BitDepthMap(k2To4Map);
  space.Set2To8BitDepthMap(k2To8Map);
  space.Set4To8BitDepthMap(k4To8Map);
  space.SetColor(BitDepth::k2Bit, 0x0, kRed);
  space.SetColor(BitDepth::k4Bit, 0x0, kGreen);
  space.SetColor(BitDepth::k4Bit, 0x6, kBlue);
  space.SetColor(BitDepth::k8Bit, 0x0, kBlack);
  space.SetColor(BitDepth::k8Bit, 0xb, kWhite);
  space.SetColor(BitDepth::k8Bit, 0x12, kYellow);

  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0x0), kRed);     // Exact match
  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0x1), kBlue);    // 0x06 in 4-bit
  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0x2), kWhite);   // 0xb in 8-bit
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x0), kGreen);   // Exact match
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x1), kYellow);  // 0x12 in 8-bit
}

TEST(DvbImageColorSpaceTest, HandlesDefaultColors) {
  DvbImageColorSpace space;

  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0x0).a, 0u);  // Only T is spec'd
  EXPECT_EQ(space.GetColor(BitDepth::k2Bit, 0x2), (RgbaColor{0, 0, 0, 255}));

  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x0).a, 0u);  // Only T is spec'd
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x1), (RgbaColor{255, 0, 0, 255}));
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x2), (RgbaColor{0, 255, 0, 255}));
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x5),
            (RgbaColor{255, 0, 255, 255}));
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0x9), (RgbaColor{127, 0, 0, 255}));
  EXPECT_EQ(space.GetColor(BitDepth::k4Bit, 0xa), (RgbaColor{0, 127, 0, 255}));

  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x0).a, 0u);  // Only T is spec'd
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x1), (RgbaColor{255, 0, 0, 63}));
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x3), (RgbaColor{255, 255, 0, 63}));
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x61),
            (RgbaColor{84, 170, 170, 255}));
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x46),
            (RgbaColor{0, 84, 255, 255}));
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0x1a),
            (RgbaColor{170, 84, 0, 127}));
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0xf2),
            (RgbaColor{211, 255, 211, 255}));
  EXPECT_EQ(space.GetColor(BitDepth::k8Bit, 0xbe),
            (RgbaColor{84, 127, 43, 255}));
}

TEST(DvbImageBuilderTest, BasicFlow) {
  DvbImageColorSpace colors;
  FillDefaultColorSpace(&colors);
  const uint16_t kWidth = 4;

  DvbImageBuilder image(&colors, kBlack, kWidth, 5);
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kGreenId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kWhiteId, kTopRow));

  ASSERT_TRUE(AddPixelRow(&image, kWidth, kBlueId, kBottomRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kBlackId, kBottomRow));

  CheckImagePixels(&image, kWidth, {kRed, kBlue, kGreen, kBlack, kWhite});
}

TEST(DvbImageBuilderTest, AllowsSmallerImages) {
  DvbImageColorSpace colors;
  FillDefaultColorSpace(&colors);
  const uint16_t kWidth = 4;

  DvbImageBuilder image(&colors, kBlack, kWidth + 10, 5);
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kGreenId, kBottomRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kBlueId, kTopRow));

  CheckImagePixels(&image, kWidth, {kRed, kGreen, kBlue});
}

TEST(DvbImageBuilderTest, ValidatesMaxWidth) {
  DvbImageColorSpace colors;
  FillDefaultColorSpace(&colors);
  const uint16_t kWidth = 4;

  DvbImageBuilder image(&colors, kBlack, kWidth, 5);
  for (size_t i = 0; i < kWidth; i++)
    ASSERT_TRUE(image.AddPixel(BitDepth::k8Bit, kRedId, kTopRow));
  // Cannot exceed max_width on first line.
  ASSERT_FALSE(image.AddPixel(BitDepth::k8Bit, kRedId, kTopRow));
  // Despite the error, the image should still be in the same state as before.
  image.NewRow(kTopRow);
  for (size_t i = 0; i < kWidth; i++)
    ASSERT_TRUE(image.AddPixel(BitDepth::k8Bit, kRedId, kTopRow));
  // Cannot exceed max_width on other lines.
  ASSERT_FALSE(image.AddPixel(BitDepth::k8Bit, kRedId, kTopRow));
}

TEST(DvbImageBuilderTest, SupportsInconsistentWidths) {
  DvbImageColorSpace colors;
  FillDefaultColorSpace(&colors);

  DvbImageBuilder image(&colors, kBlack, 10, 10);
  ASSERT_TRUE(image.AddPixel(BitDepth::k8Bit, kRedId, kTopRow));
  ASSERT_TRUE(image.AddPixel(BitDepth::k8Bit, kRedId, kTopRow));
  ASSERT_TRUE(image.AddPixel(BitDepth::k8Bit, kRedId, kTopRow));
  image.NewRow(kTopRow);
  ASSERT_TRUE(image.AddPixel(BitDepth::k8Bit, kBlueId, kBottomRow));
  ASSERT_TRUE(image.AddPixel(BitDepth::k8Bit, kBlueId, kBottomRow));
  image.NewRow(kBottomRow);
  ASSERT_TRUE(image.AddPixel(BitDepth::k8Bit, kYellowId, kTopRow));
  image.NewRow(kTopRow);

  const RgbaColor* pixels;
  uint16_t width, height;
  ASSERT_TRUE(image.GetPixels(&pixels, &width, &height));
  EXPECT_EQ(width, 3);
  EXPECT_EQ(height, 3);

  EXPECT_EQ(pixels[0], kRed);
  EXPECT_EQ(pixels[1], kRed);
  EXPECT_EQ(pixels[2], kRed);
  EXPECT_EQ(pixels[10], kBlue);
  EXPECT_EQ(pixels[11], kBlue);
  EXPECT_EQ(pixels[12], kBlack);
  EXPECT_EQ(pixels[20], kYellow);
  EXPECT_EQ(pixels[21], kBlack);
  EXPECT_EQ(pixels[22], kBlack);
}

TEST(DvbImageBuilderTest, ValidatesTotalLength) {
  DvbImageColorSpace colors;
  FillDefaultColorSpace(&colors);
  const uint16_t kWidth = 4;

  DvbImageBuilder image(&colors, kBlack, kWidth, 3);
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kBottomRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));

  ASSERT_FALSE(image.AddPixel(BitDepth::k8Bit, kRedId, kTopRow));
  ASSERT_FALSE(image.AddPixel(BitDepth::k8Bit, kRedId, kBottomRow));
}

TEST(DvbImageBuilderTest, ValidatesTopBottomFieldsMatch) {
  DvbImageColorSpace colors;
  FillDefaultColorSpace(&colors);
  const uint16_t kWidth = 4;

  DvbImageBuilder image(&colors, kBlack, kWidth, 5);
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kBottomRow));
  // Not enough bottom rows.

  uint16_t width, height;
  const RgbaColor* pixels;
  ASSERT_FALSE(image.GetPixels(&pixels, &width, &height));
}

TEST(DvbImageBuilderTest, MirrorToBottomRowsEven) {
  DvbImageColorSpace colors;
  FillDefaultColorSpace(&colors);
  const uint16_t kWidth = 4;
  const uint16_t kHeight = 4;

  DvbImageBuilder image(&colors, kBlack, kWidth, kHeight);
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kGreenId, kTopRow));
  image.MirrorToBottomRows();

  CheckImagePixels(&image, kWidth, {kRed, kRed, kGreen, kGreen});
}

TEST(DvbImageBuilderTest, MirrorToBottomRowsOdd) {
  DvbImageColorSpace colors;
  FillDefaultColorSpace(&colors);
  const uint16_t kWidth = 4;
  const uint16_t kHeight = 5;

  DvbImageBuilder image(&colors, kBlack, kWidth, kHeight);
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kRedId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kGreenId, kTopRow));
  ASSERT_TRUE(AddPixelRow(&image, kWidth, kBlueId, kTopRow));
  image.MirrorToBottomRows();

  CheckImagePixels(&image, kWidth, {kRed, kRed, kGreen, kGreen, kBlue});
}

}  // namespace media
}  // namespace shaka
