// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/decoding_time_iterator.h>

#include <memory>

#include <gtest/gtest.h>

#include <packager/macros/classes.h>

namespace shaka {
namespace media {
namespace mp4 {

const DecodingTime kDecodingTimes[] =
    {{10, 8}, {9, 5}, {25, 7}, {48, 63}, {8, 2}};

class DecodingTimeIteratorTest : public testing::Test {
 public:
  DecodingTimeIteratorTest() {
    // Build decoding time table from kDecodingTimes.
    int32_t decoding_time = 0;
    uint32_t length = sizeof(kDecodingTimes) / sizeof(DecodingTime);
    for (uint32_t i = 0; i < length; ++i) {
      for (uint32_t j = 0; j < kDecodingTimes[i].sample_count; ++j) {
        decoding_time += kDecodingTimes[i].sample_delta;
        decoding_time_table_.push_back(decoding_time);
      }
    }

    decoding_time_to_sample_.decoding_time.assign(kDecodingTimes,
                                                  kDecodingTimes + length);
    decoding_time_iterator_.reset(
        new DecodingTimeIterator(decoding_time_to_sample_));
  }

 protected:
  std::vector<uint32_t> decoding_time_table_;
  DecodingTimeToSample decoding_time_to_sample_;
  std::unique_ptr<DecodingTimeIterator> decoding_time_iterator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecodingTimeIteratorTest);
};

TEST_F(DecodingTimeIteratorTest, EmptyDecodingTime) {
  DecodingTimeToSample decoding_time_to_sample;
  DecodingTimeIterator iterator(decoding_time_to_sample);
  EXPECT_FALSE(iterator.IsValid());
  EXPECT_EQ(0u, iterator.NumSamples());
}

TEST_F(DecodingTimeIteratorTest, NumSamples) {
  ASSERT_EQ(decoding_time_table_.size(), decoding_time_iterator_->NumSamples());
}

TEST_F(DecodingTimeIteratorTest, AdvanceSample) {
  ASSERT_EQ(decoding_time_table_[0], decoding_time_iterator_->sample_delta());
  for (uint32_t sample = 1; sample < decoding_time_table_.size(); ++sample) {
    ASSERT_TRUE(decoding_time_iterator_->AdvanceSample());
    ASSERT_EQ(decoding_time_table_[sample] - decoding_time_table_[sample - 1],
              decoding_time_iterator_->sample_delta());
    ASSERT_TRUE(decoding_time_iterator_->IsValid());
  }
  ASSERT_FALSE(decoding_time_iterator_->AdvanceSample());
  ASSERT_FALSE(decoding_time_iterator_->IsValid());
}

TEST_F(DecodingTimeIteratorTest, Duration) {
  for (uint32_t i = 0; i < decoding_time_table_.size(); ++i) {
    for (uint32_t j = i; j < decoding_time_table_.size(); ++j) {
      ASSERT_EQ(
          decoding_time_table_[j] - (i == 0 ? 0 : decoding_time_table_[i - 1]),
          decoding_time_iterator_->Duration(i + 1, j + 1));
    }
  }
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
