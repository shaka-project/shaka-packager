// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/err.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"

namespace {

bool CheckExpansionCase(const char* input, const char* expected, bool success) {
  Scope scope(static_cast<const Settings*>(NULL));
  scope.SetValue("one", Value(NULL, 1), NULL);
  scope.SetValue("onestring", Value(NULL, "one"), NULL);

  // Construct the string token, which includes the quotes.
  std::string literal_string;
  literal_string.push_back('"');
  literal_string.append(input);
  literal_string.push_back('"');
  Token literal(Location(), Token::STRING, literal_string);

  Value result(NULL, Value::STRING);
  Err err;
  bool ret = ExpandStringLiteral(&scope, literal, &result, &err);

  // Err and return value should agree.
  EXPECT_NE(ret, err.has_error());

  if (ret != success)
    return false;

  if (!success)
    return true;  // Don't check result on failure.
  return result.string_value() == expected;
}

}  // namespace

TEST(StringUtils, ExpandStringLiteral) {
  EXPECT_TRUE(CheckExpansionCase("", "", true));
  EXPECT_TRUE(CheckExpansionCase("hello", "hello", true));
  EXPECT_TRUE(CheckExpansionCase("hello #$one", "hello #1", true));
  EXPECT_TRUE(CheckExpansionCase("hello #$one/two", "hello #1/two", true));
  EXPECT_TRUE(CheckExpansionCase("hello #${one}", "hello #1", true));
  EXPECT_TRUE(CheckExpansionCase("hello #${one}one", "hello #1one", true));
  EXPECT_TRUE(CheckExpansionCase("hello #${one}$one", "hello #11", true));
  EXPECT_TRUE(CheckExpansionCase("$onestring${one}$one", "one11", true));

  // Errors
  EXPECT_TRUE(CheckExpansionCase("hello #$", NULL, false));
  EXPECT_TRUE(CheckExpansionCase("hello #$%", NULL, false));
  EXPECT_TRUE(CheckExpansionCase("hello #${", NULL, false));
  EXPECT_TRUE(CheckExpansionCase("hello #${}", NULL, false));
  EXPECT_TRUE(CheckExpansionCase("hello #$nonexistant", NULL, false));
  EXPECT_TRUE(CheckExpansionCase("hello #${unterminated", NULL, false));

  // Unknown backslash values aren't special.
  EXPECT_TRUE(CheckExpansionCase("\\", "\\", true));
  EXPECT_TRUE(CheckExpansionCase("\\b", "\\b", true));

  // Backslashes escape some special things. \"\$\\ -> "$\  Note that gtest
  // doesn't like this escape sequence so we have to put it out-of-line.
  const char* in = "\\\"\\$\\\\";
  const char* out = "\"$\\";
  EXPECT_TRUE(CheckExpansionCase(in, out, true));
}
