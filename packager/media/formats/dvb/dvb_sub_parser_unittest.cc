// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/dvb/dvb_sub_parser.h>

#include <string>
#include <utility>
#include <vector>

#include <absl/log/check.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shaka {
namespace media {

namespace {

constexpr const uint8_t kRegionId = 7;
constexpr const uint8_t kClutId = 12;
constexpr const uint8_t kObjectId1 = 1;
constexpr const uint8_t kObjectId2 = 2;

constexpr const int64_t kNoPts = 0;

/// @param object_id The Object ID.
/// @param pairs A vector of data_type plus data body.  Each pair should be a
///        single row.  It should be image order (i.e. not interlaced).  The
///        body is an array of strings containing binary codes (e.g. "01").
std::vector<uint8_t> GenerateObjectData(
    uint8_t object_id,
    const std::vector<std::pair<uint8_t, std::vector<std::string>>>& pairs) {
  std::vector<uint8_t> ret;
  ret.push_back(0);
  ret.push_back(object_id);
  ret.push_back(0);
  ret.insert(ret.end(), 4, 0);  // insert dummy bytes for size

  auto push_data = [&](size_t start_index) {
    const auto start_size = ret.size();
    for (size_t i = start_index; i < pairs.size(); i += 2) {
      ret.push_back(pairs[i].first);

      uint8_t temp = 0;
      uint8_t count = 0;
      for (const auto& str : pairs[i].second) {
        for (const auto& ch : str) {
          if (ch == ' ')
            continue;

          CHECK(ch == '0' || ch == '1');
          temp = (temp << 1) | (ch - '0');
          if (++count == 8) {
            ret.push_back(temp);
            temp = count = 0;
          }
        }
      }
      if (count != 0)
        ret.push_back(temp << (8 - count));
      ret.push_back(0xf0);  // end-of-line
    }
    return ret.size() - start_size;
  };

  const size_t top_size = push_data(0);
  const size_t bottom_size = push_data(1);
  CHECK(top_size <= 0xffff);
  CHECK(bottom_size <= 0xffff);
  ret[3] = (top_size >> 8) & 0xff;
  ret[4] = top_size & 0xff;
  ret[5] = (bottom_size >> 8) & 0xff;
  ret[6] = bottom_size & 0xff;

  return ret;
}

}  // namespace

class DvbSubParserTest : public testing::Test {
 protected:
  const DvbImageColorSpace* GetColorSpace(DvbSubParser* parser,
                                          uint8_t clut_id) {
    return parser->GetColorSpace(clut_id);
  }
  const DvbImageBuilder* GetImage(DvbSubParser* parser, uint8_t object_id) {
    return parser->GetImageForObject(object_id);
  }
};

TEST_F(DvbSubParserTest, TestHelper) {
  const uint8_t kResult[] = {
      // clang-format off
      0x00, 0x12, 0x00,

      0x00, 0x09,  // top-rows size
      0x00, 0x04,  // bottom-rows size

      // Top-rows
      0x30, 0x1b, 0xe9, 0x6b,
      0xf0,
      0x88, 0xc8, 0xe0,
      0xf0,

      // Bottom-rows
      0x11, 0xe6, 0xcd,
      0xf0,
      // clang-format on
  };
  std::vector<uint8_t> expected(kResult, kResult + sizeof(kResult));

  const auto actual =
      GenerateObjectData(0x12, {{0x30, {"00011011", "11101001", "01101011"}},
                                {0x11, {"11100110", "11001101"}},
                                {0x88, {"11", "00", "10", "00", "11", "10"}}});
  EXPECT_EQ(actual, expected);
}

TEST_F(DvbSubParserTest, BasicFlow) {
  // Note up to segment_length is handled by caller.
  constexpr const uint8_t kDisplayDefinitionSegment[] = {
      0x00,      // dds_version_number(4) | display_window_flag(1) | reserved(3)
      0x00, 99,  // display_width
      0x00, 99,  // display_height
  };
  constexpr const uint8_t kPageCompositionSegment[] = {
      0x02,  // page_time_out
      0x04,  // page_version_number(4) | page_state(2) | reserved(2)
      // First region
      kRegionId,   // region_id
      0x00,        // reserved
      0x00, 0x11,  // region_horizontal_address
      0x00, 0x12,  // region_vertical_address
  };
  constexpr const uint8_t kRegionCompositionSegment[] = {
      kRegionId,  // region_id
      0x08,       // region_version_number(4) | region_fill_flag(1) |
                  //   reserved(3)
      0x00, 50,   // region_width
      0x00, 50,   // region_height
      0x6c,       // region_level_of_compatibility(3) | region_depth(3) |
                  //   reserved(2)
      kClutId,    // CLUT_id
      0x02,       // region_8-bit_pixel_code,
      0x28,       // region_4-bit_pixel_code(4) | region_2-bit_pixel_code(2) |
                  //   reserved(2)

      // First object
      0x00, kObjectId1,  // object_id
      0x00, 0x07,        // object_type(2) | object_provider_flag(2) |
                         //   object_horizontal_position(12)
      0x00, 0x08,        // reserved(4) | object_vertical_position(12)

      // Second object
      0x00, kObjectId2,  // object_id
      0x00, 0x09,        // object_type(2) | object_provider_flag(2) |
                         //   object_horizontal_position(12)
      0x00, 0x0c,        // reserved(4) | object_vertical_position(12)
  };
  constexpr const uint8_t kClutDefinitionSegment[] = {
      // clang-format off
      kClutId,  // CLUT_id
      0x00,     // CLUT_version_number(4) | reserved(4)

      // First color
      0x00,  // CLUT_entry_id
      0x81,  // flags (2-bit,full-range)
      70, 141, 117, 0,
      0x00,  // CLUT_entry_id
      0x41,  // flags (4-bit,full-range)
      70, 141, 117, 0,
      0x00,  // CLUT_entry_id
      0x21,  // flags (8-bit,full-range)
      70, 141, 117, 0,

      // Second color
      0x01,  // CLUT_entry_id
      0x81,  // flags (2-bit,full-range)
      33, 134, 122, 0,
      0x01,  // CLUT_entry_id
      0x41,  // flags (4-bit,full-range)
      33, 134, 122, 0,
      0x01,  // CLUT_entry_id
      0x21,  // flags (8-bit,full-range)
      33, 134, 122, 0,

      // Third color
      0x02,  // CLUT_entry_id
      0x81,  // flags (2-bit,full-range)
      100, 128, 127, 0,
      0x02,  // CLUT_entry_id
      0x41,  // flags (4-bit,full-range)
      100, 128, 127, 0,
      0x02,  // CLUT_entry_id
      0x21,  // flags (8-bit,full-range)
      100, 128, 127, 0,
      // clang-format on
  };
  // 0 0 0 0 1 1
  // 0 1 1 1 0 0
  // 1 0 1 1 1 1
  // 0 0 0 1
  const auto kObjectData1 = GenerateObjectData(
      kObjectId1,
      {
          {0x10, {"00100100", "01", "01", "000000"}},
          {0x10, {"0001", "00100001", "000001", "000000"}},
          {0x11, {"0001", "0000 1100", "0000 1000 0001", "0000 0000"}},
          {0x11, {"0000 0001", "0001", "0000 0000"}},
      });
  // 1 1 0 0
  // 0 0 1 0
  // 1 0 0 0
  const uint8_t kObjectData2[] = {
      0x00, kObjectId2, 0x00,

      0x00, 0x0f,  // top-rows length
      0x00, 0x09,  // bottom-rows length

      0x12, 0x01,       0x01, 0x00, 0x02, 0x00, 0x00, 0xf0,  // row 0
      0x12, 0x01,       0x00, 0x03, 0x00, 0x00, 0xf0,        // row 2

      0x12, 0x00,       0x02, 0x01, 0x00, 0x01, 0x00, 0x00, 0xf0,  // row 1
  };
  constexpr const uint8_t kEndOfDisplaySegment[] = {0x00};
  auto check_image_data = [&](DvbSubParser* parser, uint8_t object_id,
                              const std::vector<uint8_t>& data) {
    const RgbaColor* pixels;
    uint16_t width, height;
    auto* color_space = GetColorSpace(parser, kClutId);
    auto* image = GetImage(parser, object_id);
    ASSERT_TRUE(image);
    ASSERT_TRUE(color_space);
    ASSERT_TRUE(image->GetPixels(&pixels, &width, &height));
    ASSERT_EQ(static_cast<size_t>(width * height), data.size());
    for (size_t y = 0; y < height; y++) {
      for (size_t x = 0; x < width; x++) {
        auto color =
            color_space->GetColor(BitDepth::k8Bit, data[x + y * width]);
        EXPECT_EQ(pixels[x + y * image->max_width()], color)
            << "Object=" << static_cast<int>(object_id) << ", X=" << x
            << ", Y=" << y;
      }
    }
  };

  DvbSubParser parser;
  std::vector<std::shared_ptr<TextSample>> samples;
  ASSERT_TRUE(parser.Parse(DvbSubSegmentType::kDisplayDefinition, kNoPts,
                           kDisplayDefinitionSegment,
                           sizeof(kDisplayDefinitionSegment), &samples));
  ASSERT_TRUE(parser.Parse(DvbSubSegmentType::kPageComposition, kNoPts,
                           kPageCompositionSegment,
                           sizeof(kPageCompositionSegment), &samples));
  ASSERT_TRUE(parser.Parse(DvbSubSegmentType::kRegionComposition, kNoPts,
                           kRegionCompositionSegment,
                           sizeof(kRegionCompositionSegment), &samples));
  ASSERT_TRUE(parser.Parse(DvbSubSegmentType::kClutDefinition, kNoPts,
                           kClutDefinitionSegment,
                           sizeof(kClutDefinitionSegment), &samples));
  ASSERT_TRUE(parser.Parse(DvbSubSegmentType::kObjectData, kNoPts,
                           kObjectData1.data(), kObjectData1.size(), &samples));
  ASSERT_TRUE(parser.Parse(DvbSubSegmentType::kObjectData, kNoPts, kObjectData2,
                           sizeof(kObjectData2), &samples));
  ASSERT_TRUE(parser.Parse(DvbSubSegmentType::kEndOfDisplay, kNoPts,
                           kEndOfDisplaySegment, sizeof(kEndOfDisplaySegment),
                           &samples));

  check_image_data(&parser, kObjectId1, {0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0,
                                         1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 2, 2});
  check_image_data(&parser, kObjectId2, {1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0});

  ASSERT_TRUE(parser.Flush(&samples));
  ASSERT_EQ(samples.size(), 2u);
  for (auto& sample : samples) {
    ASSERT_TRUE(sample->settings().line);
    ASSERT_EQ(sample->settings().line->type, TextUnitType::kPercent);
    ASSERT_TRUE(sample->settings().position);
    ASSERT_EQ(sample->settings().position->type, TextUnitType::kPercent);
    ASSERT_TRUE(sample->settings().width);
    ASSERT_EQ(sample->settings().width->type, TextUnitType::kPercent);
    ASSERT_TRUE(sample->settings().height);
    ASSERT_EQ(sample->settings().height->type, TextUnitType::kPercent);

    ASSERT_FALSE(sample->body().image.empty());
  }
  // Allow in either order.
  if (samples[0]->settings().position->value == 0x1a)
    std::swap(samples[0], samples[1]);

  EXPECT_EQ(samples[0]->settings().position->value, 0x18);
  EXPECT_EQ(samples[0]->settings().line->value, 0x1a);
  EXPECT_EQ(samples[0]->settings().width->value, 6);
  EXPECT_EQ(samples[0]->settings().height->value, 4);

  EXPECT_EQ(samples[1]->settings().position->value, 0x1a);
  EXPECT_EQ(samples[1]->settings().line->value, 0x1e);
  EXPECT_EQ(samples[1]->settings().width->value, 4);
  EXPECT_EQ(samples[1]->settings().height->value, 3);
}

}  // namespace media
}  // namespace shaka
