// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/string_escape.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const struct json_narrow_test_data {
  const char* to_escape;
  const char* escaped;
} json_narrow_cases[] = {
  {"\b\001aZ\"\\wee", "\\b\\u0001aZ\\\"\\\\wee"},
  {"a\b\f\n\r\t\v\1\\.\"z",
      "a\\b\\f\\n\\r\\t\\u000B\\u0001\\\\.\\\"z"},
  {"b\x0f\x7f\xf0\xff!", "b\\u000F\\u007F\\u00F0\\u00FF!"},
  {"c<>d", "c\\u003C\\u003Ed"},
};

}  // namespace

TEST(StringEscapeTest, JsonDoubleQuoteNarrow) {
  for (size_t i = 0; i < arraysize(json_narrow_cases); ++i) {
    std::string in = json_narrow_cases[i].to_escape;
    std::string out;
    JsonDoubleQuote(in, false, &out);
    EXPECT_EQ(std::string(json_narrow_cases[i].escaped), out);
  }

  std::string in = json_narrow_cases[0].to_escape;
  std::string out;
  JsonDoubleQuote(in, false, &out);

  // test quoting
  std::string out_quoted;
  JsonDoubleQuote(in, true, &out_quoted);
  EXPECT_EQ(out.length() + 2, out_quoted.length());
  EXPECT_EQ(out_quoted.find(out), 1U);

  // now try with a NULL in the string
  std::string null_prepend = "test";
  null_prepend.push_back(0);
  in = null_prepend + in;
  std::string expected = "test\\u0000";
  expected += json_narrow_cases[0].escaped;
  out.clear();
  JsonDoubleQuote(in, false, &out);
  EXPECT_EQ(expected, out);
}

namespace {

const struct json_wide_test_data {
  const wchar_t* to_escape;
  const char* escaped;
} json_wide_cases[] = {
  {L"b\uffb1\u00ff", "b\\uFFB1\\u00FF"},
  {L"\b\001aZ\"\\wee", "\\b\\u0001aZ\\\"\\\\wee"},
  {L"a\b\f\n\r\t\v\1\\.\"z",
      "a\\b\\f\\n\\r\\t\\u000B\\u0001\\\\.\\\"z"},
  {L"b\x0f\x7f\xf0\xff!", "b\\u000F\\u007F\\u00F0\\u00FF!"},
  {L"c<>d", "c\\u003C\\u003Ed"},
};

}  // namespace

TEST(StringEscapeTest, JsonDoubleQuoteWide) {
  for (size_t i = 0; i < arraysize(json_wide_cases); ++i) {
    std::string out;
    string16 in = WideToUTF16(json_wide_cases[i].to_escape);
    JsonDoubleQuote(in, false, &out);
    EXPECT_EQ(std::string(json_wide_cases[i].escaped), out);
  }

  string16 in = WideToUTF16(json_wide_cases[0].to_escape);
  std::string out;
  JsonDoubleQuote(in, false, &out);

  // test quoting
  std::string out_quoted;
  JsonDoubleQuote(in, true, &out_quoted);
  EXPECT_EQ(out.length() + 2, out_quoted.length());
  EXPECT_EQ(out_quoted.find(out), 1U);

  // now try with a NULL in the string
  string16 null_prepend = WideToUTF16(L"test");
  null_prepend.push_back(0);
  in = null_prepend + in;
  std::string expected = "test\\u0000";
  expected += json_wide_cases[0].escaped;
  out.clear();
  JsonDoubleQuote(in, false, &out);
  EXPECT_EQ(expected, out);
}

}  // namespace base
