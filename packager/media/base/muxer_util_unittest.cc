// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/base/muxer_util.h"

namespace shaka {
namespace media {

TEST(MuxerUtilTest, ValidateSegmentTemplate) {
  EXPECT_FALSE(ValidateSegmentTemplate(""));

  EXPECT_TRUE(ValidateSegmentTemplate("$Number$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Time$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$DecodeTime$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Time$$Time$"));
  EXPECT_TRUE(ValidateSegmentTemplate("foo$Time$goo"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Number$_$Number$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Number$$Bandwidth$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$Time$$Bandwidth$"));

  // Bandwidth without Number or Time should not be valid.
  EXPECT_FALSE(ValidateSegmentTemplate("$Bandwidth$"));

  // Escape sequence "$$".
  EXPECT_TRUE(ValidateSegmentTemplate("foo$Time$__$$loo"));
  EXPECT_TRUE(ValidateSegmentTemplate("foo$Time$$$"));
  EXPECT_TRUE(ValidateSegmentTemplate("$$$Time$$$"));

  // Missing $Number$ / $Time$ / $DecodeTime$.
  EXPECT_FALSE(ValidateSegmentTemplate("$$"));
  EXPECT_FALSE(ValidateSegmentTemplate("foo$$goo"));

  // $Number$, $Time$ should not co-exist.
  EXPECT_FALSE(ValidateSegmentTemplate("$Number$$Time$"));
  EXPECT_FALSE(ValidateSegmentTemplate("foo$Number$_$Time$loo"));

  // $RepresentationID$ not implemented yet.
  EXPECT_FALSE(ValidateSegmentTemplate("$RepresentationID$__$Time$"));

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
  const uint64_t kSegmentStartTime = 180180;
  const uint64_t kSegmentStartDecodeTime = 180173;
  const uint32_t kSegmentIndex = 11;
  const uint32_t kBandwidth = 1234;
  EXPECT_EQ("12", GetSegmentName("$Number$",
                                 kSegmentStartTime,
                                 kSegmentStartTime,
                                 kSegmentIndex,
                                 kBandwidth));
  EXPECT_EQ("012",
            GetSegmentName("$Number%03d$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));
  EXPECT_EQ(
      "12$foo$00012",
      GetSegmentName(
          "$Number%01d$$$foo$$$Number%05d$",
          kSegmentStartTime,
          kSegmentStartDecodeTime,
          kSegmentIndex,
          kBandwidth));

  EXPECT_EQ("180180",
            GetSegmentName("$Time$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));
  EXPECT_EQ("180173",
            GetSegmentName("$DecodeTime$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));
  EXPECT_EQ("foo$_$18018000180180.m4s",
            GetSegmentName("foo$$_$$$Time%01d$$Time%08d$.m4s",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));

  // Combo values.
  EXPECT_EQ("12-1234",
            GetSegmentName("$Number$-$Bandwidth$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));
  EXPECT_EQ("012-001234",
            GetSegmentName("$Number%03d$-$Bandwidth%06d$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));

  // Format specifier edge cases.
  EXPECT_EQ("12",
            GetSegmentName("$Number%00d$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));
  EXPECT_EQ("00012",
            GetSegmentName("$Number%005d$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));
}

TEST(MuxerUtilTest, GetSegmentNameWithIndexZero) {
  const uint64_t kSegmentStartTime = 0;
  const uint64_t kSegmentStartDecodeTime = 0;
  const uint32_t kSegmentIndex = 0;
  const uint32_t kBandwidth = 0;
  EXPECT_EQ("1", GetSegmentName("$Number$",
                                kSegmentStartTime,
                                kSegmentStartDecodeTime,
                                kSegmentIndex,
                                kBandwidth));
  EXPECT_EQ("001",
            GetSegmentName("$Number%03d$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));

  EXPECT_EQ("0", GetSegmentName("$Time$",
                                kSegmentStartTime,
                                kSegmentStartDecodeTime,
                                kSegmentIndex,
                                kBandwidth));
  EXPECT_EQ("00000000.m4s",
            GetSegmentName("$Time%08d$.m4s",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));
}

TEST(MuxerUtilTest, GetSegmentNameLargeTime) {
  const uint64_t kSegmentStartTime = 1601599839840ULL;
  const uint64_t kSegmentStartDecodeTime = 1337;
  const uint32_t kSegmentIndex = 8888888;
  const uint32_t kBandwidth = 444444;
  EXPECT_EQ("1601599839840",
            GetSegmentName("$Time$",
                           kSegmentStartTime,
                           kSegmentStartDecodeTime,
                           kSegmentIndex,
                           kBandwidth));
}

}  // namespace media
}  // namespace shaka
