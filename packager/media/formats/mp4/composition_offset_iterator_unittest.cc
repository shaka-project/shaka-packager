// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/composition_offset_iterator.h>

#include <memory>

#include <gtest/gtest.h>

#include <packager/macros/classes.h>

namespace shaka {
namespace media {
namespace mp4 {

const CompositionOffset kCompositionOffsets[] =
    {{10, -8}, {9, 5}, {25, 7}, {48, 63}, {8, 2}};

class CompositionOffsetIteratorTest : public testing::Test {
 public:
  CompositionOffsetIteratorTest() {
    // Build composition offset table from kCompositionOffsets.
    uint32_t length = sizeof(kCompositionOffsets) / sizeof(CompositionOffset);
    for (uint32_t i = 0; i < length; ++i) {
      for (uint32_t j = 0; j < kCompositionOffsets[i].sample_count; ++j) {
        composition_offset_table_.push_back(
            kCompositionOffsets[i].sample_offset);
      }
    }

    composition_time_to_sample_.composition_offset.assign(
        kCompositionOffsets, kCompositionOffsets + length);
    composition_offset_iterator_.reset(
        new CompositionOffsetIterator(composition_time_to_sample_));
  }

 protected:
  std::vector<int64_t> composition_offset_table_;
  CompositionTimeToSample composition_time_to_sample_;
  std::unique_ptr<CompositionOffsetIterator> composition_offset_iterator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositionOffsetIteratorTest);
};

TEST_F(CompositionOffsetIteratorTest, EmptyCompositionTime) {
  CompositionTimeToSample composition_time_to_sample;
  CompositionOffsetIterator iterator(composition_time_to_sample);
  EXPECT_FALSE(iterator.IsValid());
  EXPECT_EQ(0u, iterator.NumSamples());
}

TEST_F(CompositionOffsetIteratorTest, NumSamples) {
  ASSERT_EQ(composition_offset_table_.size(),
            composition_offset_iterator_->NumSamples());
}

TEST_F(CompositionOffsetIteratorTest, AdvanceSample) {
  ASSERT_EQ(composition_offset_table_[0],
            composition_offset_iterator_->sample_offset());
  for (uint32_t sample = 1; sample < composition_offset_table_.size();
       ++sample) {
    ASSERT_TRUE(composition_offset_iterator_->AdvanceSample());
    ASSERT_EQ(composition_offset_table_[sample],
              composition_offset_iterator_->sample_offset());
    ASSERT_TRUE(composition_offset_iterator_->IsValid());
  }
  ASSERT_FALSE(composition_offset_iterator_->AdvanceSample());
  ASSERT_FALSE(composition_offset_iterator_->IsValid());
}

TEST_F(CompositionOffsetIteratorTest, SampleOffset) {
  for (uint32_t sample = 0; sample < composition_offset_table_.size();
       ++sample) {
    ASSERT_EQ(composition_offset_table_[sample],
              composition_offset_iterator_->SampleOffset(sample+1));
  }
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
