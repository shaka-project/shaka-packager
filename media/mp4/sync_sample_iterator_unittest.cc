// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/mp4/sync_sample_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
const uint32 kNumSamples = 100;
const uint32 kSyncSamples[] = {3, 10, 30, 35, 89, 97};

// Check if sample is an element in kSyncSamples.
bool InSyncSamples(uint32 sample) {
  for (uint32 i = 0; i < sizeof(kSyncSamples) / sizeof(uint32); ++i) {
    if (sample == kSyncSamples[i])
      return true;
  }
  return false;
}
}  // namespace

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
      kSyncSamples, kSyncSamples + sizeof(kSyncSamples) / sizeof(uint32));
  SyncSampleIterator iterator(sync_sample);

  uint32 i = 1;

  // Check if it is sync sample using SyncSampleIterator::AdvanceSample() and
  // SyncSampleIterator::IsSyncSample().
  for (; i <= kNumSamples / 2; ++i) {
    ASSERT_EQ(InSyncSamples(i), iterator.IsSyncSample());
    ASSERT_TRUE(iterator.AdvanceSample());
  }

  // Check if it is sync sample using SyncSampleIterator::IsSyncSample(uint32).
  // No need to advance sample for this case.
  for (; i <= kNumSamples / 2; ++i) {
    ASSERT_EQ(InSyncSamples(i), iterator.IsSyncSample(i));
  }
}

}  // namespace mp4
}  // namespace media
