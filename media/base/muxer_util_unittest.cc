// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/muxer_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(MuxerUtilTest, ValidateSegmentTemplate) {
  EXPECT_FALSE(ValidateSegmentTemplate(""));

  EXPECT_TRUE(ValidateSegmentTemplate("$Number$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Time$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Time$$Time$"));
  EXPECT_TRUE(ValidateSegmentTemplate("foo$Time$goo"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Number$_$Number$"));

  // Escape sequence "$$".
  EXPECT_TRUE(ValidateSegmentTemplate("foo$Time$__$$loo"));
  EXPECT_TRUE(ValidateSegmentTemplate("foo$Time$$$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$$$Time$$$"));

  // Missing $Number$ / $Time$.
  EXPECT_FALSE(ValidateSegmentTemplate("$$"));
  EXPECT_FALSE(ValidateSegmentTemplate("foo$$goo"));

  // $Number$, $Time$ should not co-exist.
  EXPECT_FALSE(ValidateSegmentTemplate("$Number$$Time$"));
  EXPECT_FALSE(ValidateSegmentTemplate("foo$Number$_$Time$loo"));

  // $RepresentationID$ and $Bandwidth$ not implemented yet.
  EXPECT_FALSE(ValidateSegmentTemplate("$RepresentationID$__$Time$"));
  EXPECT_FALSE(ValidateSegmentTemplate("foo$Bandwidth$$Time$"));

  // Unknown identifier.
  EXPECT_FALSE(ValidateSegmentTemplate("$foo$$Time$"));
}

TEST(MuxerUtilTest, ValidateSegmentTemplateWithFormatTag) {
  EXPECT_TRUE(ValidateSegmentTemplate("$Time%01d$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Time%05d$"));
  EXPECT_FALSE(ValidateSegmentTemplate("$Time%1d$"));
  EXPECT_FALSE(ValidateSegmentTemplate("$Time%$"));
  EXPECT_FALSE(ValidateSegmentTemplate("$Time%01$"));
  EXPECT_FALSE(ValidateSegmentTemplate("$Time%0xd$"));
  EXPECT_FALSE(ValidateSegmentTemplate("$Time%03xd$"));
  // $$ should not have any format tag.
  EXPECT_FALSE(ValidateSegmentTemplate("$%01d$$Time$"));
  // Format specifier edge cases.
  EXPECT_TRUE(ValidateSegmentTemplate("$Time%00d$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Time%005d$"));
}

TEST(MuxerUtilTest, GetSegmentName) {
  const uint64 kSegmentStartTime = 180180;
  const uint32 kSegmentIndex = 11;

  EXPECT_EQ("12", GetSegmentName("$Number$", kSegmentStartTime, kSegmentIndex));
  EXPECT_EQ("012",
            GetSegmentName("$Number%03d$", kSegmentStartTime, kSegmentIndex));
  EXPECT_EQ(
      "12$foo$00012",
      GetSegmentName(
          "$Number%01d$$$foo$$$Number%05d$", kSegmentStartTime, kSegmentIndex));

  EXPECT_EQ("180180",
            GetSegmentName("$Time$", kSegmentStartTime, kSegmentIndex));
  EXPECT_EQ("foo$_$18018000180180.m4s",
            GetSegmentName("foo$$_$$$Time%01d$$Time%08d$.m4s",
                           kSegmentStartTime,
                           kSegmentIndex));
  // Format specifier edge cases.
  EXPECT_EQ("12",
            GetSegmentName("$Number%00d$", kSegmentStartTime, kSegmentIndex));
  EXPECT_EQ("00012",
            GetSegmentName("$Number%005d$", kSegmentStartTime, kSegmentIndex));
}

TEST(MuxerUtilTest, GetSegmentNameWithIndexZero) {
  const uint64 kSegmentStartTime = 0;
  const uint32 kSegmentIndex = 0;

  EXPECT_EQ("1", GetSegmentName("$Number$", kSegmentStartTime, kSegmentIndex));
  EXPECT_EQ("001",
            GetSegmentName("$Number%03d$", kSegmentStartTime, kSegmentIndex));

  EXPECT_EQ("0", GetSegmentName("$Time$", kSegmentStartTime, kSegmentIndex));
  EXPECT_EQ("00000000.m4s",
            GetSegmentName("$Time%08d$.m4s", kSegmentStartTime, kSegmentIndex));
}

TEST(MuxerUtilTest, GetSegmentNameLargeTime) {
  const uint64 kSegmentStartTime = 1601599839840ULL;
  const uint32 kSegmentIndex = 8888888;

  EXPECT_EQ("1601599839840",
            GetSegmentName("$Time$", kSegmentStartTime, kSegmentIndex));
}

}  // namespace media
