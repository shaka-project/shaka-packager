// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include <cmath>

#include "packager/base/macros.h"
#include "packager/mpd/base/bandwidth_estimator.h"

namespace shaka {

namespace {
const size_t kNumBlocksForEstimate = 5;
const uint64_t kBitsInByte = 8;
const int kEstimateRoundError = 1;

struct Bandwidth {
  uint64_t average;
  uint64_t max;
};

}  // namespace

// Make sure that averaging of 5 blocks works, and also when there aren't all 5
// blocks.
TEST(BandwidthEstimatorTest, FiveBlocksFiveBlocksAdded) {
  BandwidthEstimator be(kNumBlocksForEstimate);
  const double kDuration = 1.0;
  const Bandwidth kExpectedResults[] = {
      // Harmonic mean of [1 * 8], [1 * 8, 2 * 8], ...
      // 8 is the number of bits in a byte and 1, 2, ... is from the loop
      // counter below.
      // Note that these are rounded up.
      {8, 8}, {11, 2 * 8}, {14, 3 * 8}, {16, 4 * 8}, {18, 5 * 8},
  };

  static_assert(kNumBlocksForEstimate == arraysize(kExpectedResults),
                "incorrect_number_of_expectations");
  for (uint64_t i = 1; i <= arraysize(kExpectedResults); ++i) {
    be.AddBlock(i, kDuration);
    EXPECT_EQ(kExpectedResults[i - 1].average, be.Estimate());
    EXPECT_EQ(kExpectedResults[i - 1].max, be.Max());
  }
}

// More practical situation where a lot of blocks get added but only the last 5
// are considered for the estimate.
TEST(BandwidthEstimatorTest, FiveBlocksNormal) {
  BandwidthEstimator be(kNumBlocksForEstimate);
  const double kDuration = 10.0;
  const uint64_t kNumBlocksToAdd = 200;
  const uint64_t kExptectedEstimate = 800;

  // Doesn't matter what gets passed to the estimator except for the last 5
  // blocks which we add kExptectedEstimate / 8 bytes per second so that the
  // estimate becomes kExptectedEstimate.
  for (uint64_t i = 1; i <= kNumBlocksToAdd; ++i) {
    if (i > kNumBlocksToAdd - kNumBlocksForEstimate) {
      be.AddBlock(kExptectedEstimate * kDuration / kBitsInByte, kDuration);
    } else {
      be.AddBlock(i, kDuration);
    }
  }

  EXPECT_NEAR(kExptectedEstimate, be.Estimate(), kEstimateRoundError);
  // All blocks are of the same bitrate, so Max is the same as average.
  EXPECT_NEAR(kExptectedEstimate, be.Max(), kEstimateRoundError);
}

TEST(BandwidthEstimatorTest, AllBlocks) {
  BandwidthEstimator be(BandwidthEstimator::kUseAllBlocks);
  const uint64_t kNumBlocksToAdd = 100;
  const double kDuration = 1.0;
  for (uint64_t i = 1; i <= kNumBlocksToAdd; ++i)
    be.AddBlock(i, kDuration);

  // The harmonic mean of 8, 16, ... , 800; rounded up.
  const uint64_t kExptectedEstimate = 155;
  EXPECT_EQ(kExptectedEstimate, be.Estimate());
  const uint64_t kMax = 100 * 8;
  EXPECT_EQ(kMax, be.Max());
}

TEST(BandwidthEstimatorTest, MaxWithSlidingWindow) {
  BandwidthEstimator be(kNumBlocksForEstimate);
  const double kDuration = 1.0 * kBitsInByte;

  // clang-format off
  const uint64_t kSizes[] = {
      // Sequence 1: Monotonic decreasing.
      10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
      // Sequence 2: Monotonic increasing.
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
      // Sequence 3: Random sequence.
      10, 1, 9, 6, 9, 5, 4, 9, 7, 8,
  };
  const uint64_t kExpectedMaxes[] = {
      // Sequence 1.
      10, 10, 10, 10, 10, 9, 8, 7, 6, 5,
      // Sequence 2.
      4, 3, 3, 4, 5, 6, 7, 8, 9, 10,
      // Sequence 3.
      10, 10, 10, 10, 10, 9, 9, 9, 9, 9,
  };
  // clang-format on

  static_assert(arraysize(kSizes) == arraysize(kExpectedMaxes),
                "incorrect_number_of_expectations");
  for (size_t i = 0; i < arraysize(kSizes); ++i) {
    be.AddBlock(kSizes[i], kDuration);
    EXPECT_EQ(kExpectedMaxes[i], be.Max());
  }
}

} // namespace shaka
