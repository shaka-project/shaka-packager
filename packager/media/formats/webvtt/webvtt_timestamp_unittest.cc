// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/formats/webvtt/webvtt_timestamp.h"

namespace shaka {
namespace media {

TEST(WebVttTimestampTest, TooShort) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("00.000", &ms));
}

TEST(WebVttTimestampTest, RightLengthButMeaningless) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("ABCDEFGHI", &ms));
}

TEST(WebVttTimestampTest, ParseHours) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampParse("12:00:00.000", &ms));
  EXPECT_EQ(43200000u, ms);
}

TEST(WebVttTimestampTest, ParseLongHours) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampParse("120:00:00.000", &ms));
  EXPECT_EQ(432000000u, ms);
}

TEST(WebVttTimestampTest, ParseMinutes) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampParse("00:12:00.000", &ms));
  EXPECT_EQ(720000u, ms);
}

TEST(WebVttTimestampTest, ParseSeconds) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampParse("00:00:12.000", &ms));
  EXPECT_EQ(12000u, ms);
}

TEST(WebVttTimestampTest, ParseMs) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampParse("00:00:00.123", &ms));
  EXPECT_EQ(123u, ms);
}

TEST(WebVttTimestampTest, ParseNoHours) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampParse("12:00.000", &ms));
  EXPECT_EQ(720000u, ms);
}

TEST(WebVttTimestampTest, FailWithShortHours) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("1:00:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortMinutes) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("00:1:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortSeconds) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("00:1.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortMs) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("00:00.01", &ms));
}

TEST(WebVttTimestampTest, FailWithNonDigit) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("00:0A:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithInvalidMinutes) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("00:79:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithInvalidSeconds) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampParse("00:00:79.000", &ms));
}

}  // namespace media
}  // namespace shaka
