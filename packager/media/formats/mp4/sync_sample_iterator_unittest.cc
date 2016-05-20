// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/formats/mp4/sync_sample_iterator.h"

namespace {
const uint32_t kNumSamples = 100;
const uint32_t kSyncSamples[] = {3, 10, 30, 35, 89, 97};

// Check if sample is an element in kSyncSamples.
bool InSyncSamples(uint32_t sample) {
  for (uint32_t i = 0; i < sizeof(kSyncSamples) / sizeof(uint32_t); ++i) {
    if (sample == kSyncSamples[i])
      return true;
  }
  return false;
}
}  // namespace

namespace shaka {
namespace media {
namespace mp4 {

TEST(SyncSampleIteratorTest, EmptySyncSample) {
  SyncSample sync_sample;
  SyncSampleIterator iterator(sync_sample);
  EXPECT_TRUE(iterator.IsSyncSample());
  EXPECT_TRUE(iterator.IsSyncSample(kNumSamples));
}

TEST(SyncSampleIteratorTest, SyncSample) {
  SyncSample sync_sample;
  sync_sample.sample_number.assign(
      kSyncSamples, kSyncSamples + sizeof(kSyncSamples) / sizeof(uint32_t));
  SyncSampleIterator iterator(sync_sample);

  uint32_t i = 1;

  // Check if it is sync sample using SyncSampleIterator::AdvanceSample() and
  // SyncSampleIterator::IsSyncSample().
  for (; i <= kNumSamples / 2; ++i) {
    ASSERT_EQ(InSyncSamples(i), iterator.IsSyncSample());
    ASSERT_TRUE(iterator.AdvanceSample());
  }

  // Check if it is sync sample using
  // SyncSampleIterator::IsSyncSample(uint32_t).
  // No need to advance sample for this case.
  for (; i <= kNumSamples / 2; ++i) {
    ASSERT_EQ(InSyncSamples(i), iterator.IsSyncSample(i));
  }
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
