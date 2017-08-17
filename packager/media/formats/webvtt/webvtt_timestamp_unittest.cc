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
  EXPECT_FALSE(WebVttTimestampToMs("00.000", &ms));
}

TEST(WebVttTimestampTest, RightLengthButMeaningless) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("ABCDEFGHI", &ms));
}

TEST(WebVttTimestampTest, ParseHours) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("12:00:00.000", &ms));
  EXPECT_EQ(43200000u, ms);
}

TEST(WebVttTimestampTest, ParseLongHours) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("120:00:00.000", &ms));
  EXPECT_EQ(432000000u, ms);
}

TEST(WebVttTimestampTest, ParseMinutes) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("00:12:00.000", &ms));
  EXPECT_EQ(720000u, ms);
}

TEST(WebVttTimestampTest, ParseSeconds) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("00:00:12.000", &ms));
  EXPECT_EQ(12000u, ms);
}

TEST(WebVttTimestampTest, ParseMs) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("00:00:00.123", &ms));
  EXPECT_EQ(123u, ms);
}

TEST(WebVttTimestampTest, ParseNoHours) {
  uint64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("12:00.000", &ms));
  EXPECT_EQ(720000u, ms);
}

TEST(WebVttTimestampTest, FailWithShortHours) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("1:00:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortMinutes) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:1:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortSeconds) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:1.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortMs) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:00.01", &ms));
}

TEST(WebVttTimestampTest, FailWithNonDigit) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:0A:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithInvalidMinutes) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:79:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithInvalidSeconds) {
  uint64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:00:79.000", &ms));
}

TEST(WebVttTimestampTest, CreatesMilliseconds) {
  EXPECT_EQ("00:00:00.123", MsToWebVttTimestamp(123));
}

TEST(WebVttTimestampTest, CreatesMillisecondsShort) {
  EXPECT_EQ("00:00:00.012", MsToWebVttTimestamp(12));
}

TEST(WebVttTimestampTest, CreateSeconds) {
  EXPECT_EQ("00:00:12.000", MsToWebVttTimestamp(12000));
}

TEST(WebVttTimestampTest, CreateSecondsShort) {
  EXPECT_EQ("00:00:01.000", MsToWebVttTimestamp(1000));
}

TEST(WebVttTimestampTest, CreateMinutes) {
  EXPECT_EQ("00:12:00.000", MsToWebVttTimestamp(720000));
}

TEST(WebVttTimestampTest, CreateMinutesShort) {
  EXPECT_EQ("00:01:00.000", MsToWebVttTimestamp(60000));
}

TEST(WebVttTimestampTest, CreateHours) {
  EXPECT_EQ("12:00:00.000", MsToWebVttTimestamp(43200000));
}

TEST(WebVttTimestampTest, CreateHoursShort) {
  EXPECT_EQ("01:00:00.000", MsToWebVttTimestamp(3600000));
}

TEST(WebVttTimestampTest, CreateHoursLong) {
  EXPECT_EQ("123:00:00.000", MsToWebVttTimestamp(442800000));
}
}  // namespace media
}  // namespace shaka
