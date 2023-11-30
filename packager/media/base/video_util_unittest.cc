// Copyright 2019 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/video_util.h>

#include <gtest/gtest.h>

using ::testing::TestWithParam;
using ::testing::Values;

namespace shaka {
namespace media {

struct SarTestData {
  uint32_t frame_width;
  uint32_t frame_height;
  uint32_t display_width;
  uint32_t display_height;
  uint32_t expected_pixel_width;
  uint32_t expected_pixel_height;
};

class VideoUtilSarTest : public TestWithParam<SarTestData> {};

TEST_P(VideoUtilSarTest, Test) {
  const SarTestData& test_data = GetParam();

  uint32_t pixel_width;
  uint32_t pixel_height;
  DerivePixelWidthHeight(test_data.frame_width, test_data.frame_height,
                         test_data.display_width, test_data.display_height,
                         &pixel_width, &pixel_height);
  EXPECT_EQ(pixel_width, test_data.expected_pixel_width);
  EXPECT_EQ(pixel_height, test_data.expected_pixel_height);
}

INSTANTIATE_TEST_CASE_P(VideoUtilSarTestInstance,
                        VideoUtilSarTest,
                        Values(SarTestData{1024, 768, 1024, 768, 1, 1},
                               SarTestData{1024, 384, 1024, 768, 1, 2},
                               SarTestData{512, 768, 1024, 768, 2, 1},
                               SarTestData{1024, 1024, 1024, 768, 4, 3},
                               SarTestData{123, 567, 1024, 768, 252, 41}));

}  // namespace media
}  // namespace shaka
