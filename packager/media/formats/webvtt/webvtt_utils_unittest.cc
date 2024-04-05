// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/formats/webvtt/webvtt_utils.h"

namespace shaka {
namespace media {

namespace {

const TextFragmentStyle kNoStyle{};

TextFragmentStyle GetItalicStyle() {
  TextFragmentStyle style;
  style.italic = true;
  return style;
}

TextFragmentStyle GetBoldStyle() {
  TextFragmentStyle style;
  style.bold = true;
  return style;
}

}  // namespace

TEST(WebVttTimestampTest, TooShort) {
  int64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00.000", &ms));
}

TEST(WebVttTimestampTest, RightLengthButMeaningless) {
  int64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("ABCDEFGHI", &ms));
}

TEST(WebVttTimestampTest, ParseHours) {
  int64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("12:00:00.000", &ms));
  EXPECT_EQ(43200000, ms);
}

TEST(WebVttTimestampTest, ParseLongHours) {
  int64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("120:00:00.000", &ms));
  EXPECT_EQ(432000000, ms);
}

TEST(WebVttTimestampTest, ParseMinutes) {
  int64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("00:12:00.000", &ms));
  EXPECT_EQ(720000, ms);
}

TEST(WebVttTimestampTest, ParseSeconds) {
  int64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("00:00:12.000", &ms));
  EXPECT_EQ(12000, ms);
}

TEST(WebVttTimestampTest, ParseMs) {
  int64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("00:00:00.123", &ms));
  EXPECT_EQ(123, ms);
}

TEST(WebVttTimestampTest, ParseNoHours) {
  int64_t ms;
  EXPECT_TRUE(WebVttTimestampToMs("12:00.000", &ms));
  EXPECT_EQ(720000, ms);
}

TEST(WebVttTimestampTest, FailWithShortHours) {
  int64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("1:00:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortMinutes) {
  int64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:1:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortSeconds) {
  int64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:1.000", &ms));
}

TEST(WebVttTimestampTest, FailWithShortMs) {
  int64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:00.01", &ms));
}

TEST(WebVttTimestampTest, FailWithNonDigit) {
  int64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:0A:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithInvalidMinutes) {
  int64_t ms;
  EXPECT_FALSE(WebVttTimestampToMs("00:79:00.000", &ms));
}

TEST(WebVttTimestampTest, FailWithInvalidSeconds) {
  int64_t ms;
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

TEST(WebVttUtilsTest, SettingsToString) {
  TextSettings settings;
  settings.region = "foo";
  settings.line = TextNumber(27, TextUnitType::kPercent);
  settings.position = TextNumber(42, TextUnitType::kPercent);
  settings.width = TextNumber(54, TextUnitType::kPercent);
  settings.writing_direction = WritingDirection::kVerticalGrowingLeft;
  settings.text_alignment = TextAlignment::kEnd;

  const auto actual = WebVttSettingsToString(settings);
  EXPECT_EQ(actual,
            "region:foo line:27% position:42% size:54% direction:rl align:end");
}

TEST(WebVttUtilsTest, TeletextSettingsToStringRemovesRegionOutputsIntegerLine) {
  TextSettings settings;
  settings.region = "ttx_9";
  settings.line = TextNumber(9.5, TextUnitType::kLines);
  settings.text_alignment = TextAlignment::kCenter;

  const auto actual = WebVttSettingsToString(settings);
  EXPECT_EQ(actual, "line:10 align:center");
}

TEST(WebVttUtilsTest, SettingsToString_IgnoresDefaults) {
  TextSettings settings;
  settings.region = "foo";
  settings.text_alignment = TextAlignment::kCenter;

  const auto actual = WebVttSettingsToString(settings);
  EXPECT_EQ(actual, "region:foo align:center");
}

TEST(WebVttUtilsTest, FragmentToString) {
  TextFragment frag(GetBoldStyle(), "Foobar");
  EXPECT_EQ(WebVttFragmentToString(frag), "<b>Foobar</b>");
}

TEST(WebVttUtilsTest, FragmentToString_PreservesTags) {
  TextFragment frag(kNoStyle, "<i>Foobar</i>");
  EXPECT_EQ(WebVttFragmentToString(frag), "<i>Foobar</i>");
}

TEST(WebVttUtilsTest, FragmentToString_ConsecutiveLeadingWhitespaces) {
  TextFragment frag(kNoStyle, "\r\n\t \r\nFoobar");
  EXPECT_EQ(WebVttFragmentToString(frag), " Foobar");
}

TEST(WebVttUtilsTest, FragmentToString_ConsecutiveTrailingWhitespaces) {
  TextFragment frag(kNoStyle, "Foobar\r\n\t \r\n");
  EXPECT_EQ(WebVttFragmentToString(frag), "Foobar ");
}

TEST(WebVttUtilsTest, FragmentToString_ConsecutiveInternalWhitespaces) {
  TextFragment frag(kNoStyle, "Hello\r\n\t \r\nWorld");
  EXPECT_EQ(WebVttFragmentToString(frag), "Hello World");
}

TEST(WebVttUtilsTest, FragmentToString_HandlesNestedFragments) {
  TextFragment frag;
  frag.sub_fragments.emplace_back(kNoStyle, "Hello ");
  frag.sub_fragments.emplace_back(kNoStyle, "World");
  EXPECT_EQ(WebVttFragmentToString(frag), "Hello World");
}

TEST(WebVttUtilsTest, FragmentToString_HandlesNestedFragmentsWithStyle) {
  TextFragment frag;
  frag.style.bold = true;
  frag.sub_fragments.emplace_back(GetItalicStyle(), "Hello");
  frag.sub_fragments.emplace_back(kNoStyle, " World");
  EXPECT_EQ(WebVttFragmentToString(frag), "<b><i>Hello</i> World</b>");
}

TEST(WebVttUtilsTest, FragmentToString_HandlesNewlines) {
  TextFragment frag;
  frag.sub_fragments.emplace_back(kNoStyle, "Hello");
  frag.sub_fragments.emplace_back(kNoStyle, true);
  frag.sub_fragments.emplace_back(kNoStyle, "World");
  EXPECT_EQ(WebVttFragmentToString(frag), "Hello\nWorld");
}

TEST(WebVttUtilsTest, FragmentToString_HandlesNewlinesWithStyle) {
  TextFragment frag;
  frag.style.bold = true;
  frag.sub_fragments.emplace_back(kNoStyle, "Hello");
  frag.sub_fragments.emplace_back(kNoStyle, true);
  frag.sub_fragments.emplace_back(kNoStyle, "World");
  EXPECT_EQ(WebVttFragmentToString(frag), "<b>Hello</b>\n<b>World</b>");
}

TEST(WebVttUtilsTest, FragmentToString_HandlesNestedNewlinesWithStyle) {
  TextFragment nested;
  nested.sub_fragments.emplace_back(kNoStyle, "Hello");
  nested.sub_fragments.emplace_back(kNoStyle, true);
  nested.sub_fragments.emplace_back(kNoStyle, "World");

  TextFragment frag;
  frag.style.bold = true;
  frag.sub_fragments.emplace_back(nested);
  frag.sub_fragments.emplace_back(kNoStyle, " Now");

  EXPECT_EQ(WebVttFragmentToString(frag), "<b>Hello</b>\n<b>World Now</b>");
}

TEST(WebVttUtilsTest, GetPreamble_BasicFlow) {
  TextStreamInfo info(0, 0, 0, kCodecWebVtt, "", "", 0, 0, "");
  info.set_css_styles("::cue { color: red; }");

  TextRegion region;
  region.width.value = 34;
  region.height = TextNumber(56, TextUnitType::kLines);
  region.window_anchor_x.value = 99;
  region.window_anchor_y.value = 12;
  region.region_anchor_x.value = 41;
  region.region_anchor_y.value = 29;
  info.AddRegion("foo", region);

  EXPECT_EQ(WebVttGetPreamble(info),
            "REGION\n"
            "id:foo\n"
            "width:34.000000%\n"
            "lines:56\n"
            "viewportanchor:99.000000%,12.000000%\n"
            "regionanchor:41.000000%,29.000000%\n"
            "\n"
            "STYLE\n"
            "::cue { color: red; }");
}

TEST(WebVttUtilsTest, GetPreamble_MultipleRegions) {
  TextStreamInfo info(0, 0, 0, kCodecWebVtt, "", "", 0, 0, "");

  TextRegion region1;
  region1.width.value = 34;
  region1.height = TextNumber(56, TextUnitType::kLines);
  region1.window_anchor_x.value = 99;
  region1.window_anchor_y.value = 12;
  region1.region_anchor_x.value = 41;
  region1.region_anchor_y.value = 29;
  info.AddRegion("r1", region1);

  TextRegion region2;
  region2.width.value = 82;
  region2.height = TextNumber(61, TextUnitType::kLines);
  region2.window_anchor_x.value = 51;
  region2.window_anchor_y.value = 62;
  region2.region_anchor_x.value = 92;
  region2.region_anchor_y.value = 78;
  info.AddRegion("r2", region2);

  EXPECT_EQ(WebVttGetPreamble(info),
            "REGION\n"
            "id:r1\n"
            "width:34.000000%\n"
            "lines:56\n"
            "viewportanchor:99.000000%,12.000000%\n"
            "regionanchor:41.000000%,29.000000%\n"
            "\n"
            "REGION\n"
            "id:r2\n"
            "width:82.000000%\n"
            "lines:61\n"
            "viewportanchor:51.000000%,62.000000%\n"
            "regionanchor:92.000000%,78.000000%");
}

TEST(WebVttUtilsTest, GetPreamble_Scroll) {
  TextStreamInfo info(0, 0, 0, kCodecWebVtt, "", "", 0, 0, "");

  TextRegion region;
  region.width.value = 37;
  region.height = TextNumber(82, TextUnitType::kLines);
  region.window_anchor_x.value = 32;
  region.window_anchor_y.value = 66;
  region.region_anchor_x.value = 95;
  region.region_anchor_y.value = 72;
  region.scroll = true;
  info.AddRegion("foo", region);

  EXPECT_EQ(WebVttGetPreamble(info),
            "REGION\n"
            "id:foo\n"
            "width:37.000000%\n"
            "lines:82\n"
            "viewportanchor:32.000000%,66.000000%\n"
            "regionanchor:95.000000%,72.000000%\n"
            "scroll:up");
}

TEST(WebVttUtilsTest, GetPreamble_OnlyStyles) {
  TextStreamInfo info(0, 0, 0, kCodecWebVtt, "", "", 0, 0, "");
  info.set_css_styles("::cue { color: red; }");

  EXPECT_EQ(WebVttGetPreamble(info),
            "STYLE\n"
            "::cue { color: red; }");
}

}  // namespace media
}  // namespace shaka
