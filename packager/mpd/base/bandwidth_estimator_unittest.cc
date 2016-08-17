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
const int kNumBlocksForEstimate = 5;
const int kFirstOneBlockForEstimate = -1;
const uint64_t kBitsInByte = 8;
const int kEstimateRoundError = 1;
}  // namespace

// Make sure that averaging of 5 blocks works, and also when there aren't all 5
// blocks.
TEST(BandwidthEstimatorTest, FiveBlocksFiveBlocksAdded) {
  BandwidthEstimator be(kNumBlocksForEstimate);
  const double kDuration = 1.0;
  const uint64_t kExpectedResults[] = {
      // Harmonic mean of [1 * 8], [1 * 8, 2 * 8], ...
      // 8 is the number of bits in a byte and 1, 2, ... is from the loop
      // counter below.
      // Note that these are rounded up.
      8,
      11,
      14,
      16,
      18
  };

  static_assert(kNumBlocksForEstimate == arraysize(kExpectedResults),
                "incorrect_number_of_expectations");
  for (uint64_t i = 1; i <= arraysize(kExpectedResults); ++i) {
    be.AddBlock(i, kDuration);
    EXPECT_EQ(kExpectedResults[i - 1], be.Estimate());
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
}

// Average all the blocks!
TEST(BandwidthEstimatorTest, AllBlocks) {
  BandwidthEstimator be(BandwidthEstimator::kUseAllBlocks);
  const uint64_t kNumBlocksToAdd = 100;
  const double kDuration = 1.0;
  for (uint64_t i = 1; i <= kNumBlocksToAdd; ++i)
    be.AddBlock(i, kDuration);

  // The harmonic mean of 8, 16, ... , 800; rounded up.
  const uint64_t kExptectedEstimate = 155;
  EXPECT_EQ(kExptectedEstimate, be.Estimate());
}

// Use only the first one.
TEST(BandwidthEstimatorTest, FirstOneBlock) {
  BandwidthEstimator be(kFirstOneBlockForEstimate);
  const double kDuration = 11.0;
  const uint64_t kExptectedEstimate = 123456;
  be.AddBlock(kExptectedEstimate * kDuration / kBitsInByte, kDuration);

  // Anything. Should be ignored.
  for (int i = 0; i < 1000; ++i)
    be.AddBlock(100000, 10);
  EXPECT_EQ(kExptectedEstimate, be.Estimate());
}

} // namespace shaka
