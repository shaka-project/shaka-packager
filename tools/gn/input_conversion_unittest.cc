// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/err.h"
#include "tools/gn/input_conversion.h"
#include "tools/gn/value.h"

TEST(InputConversion, String) {
  Err err;
  std::string input("\nfoo bar  \n");
  Value result = ConvertInputToValue(input, NULL, Value(NULL, "string"), &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(Value::STRING, result.type());
  EXPECT_EQ(input, result.string_value());
}

TEST(InputConversion, ListLines) {
  Err err;
  std::string input("\nfoo\nbar  \n");
  Value result = ConvertInputToValue(input, NULL, Value(NULL, "list lines"),
                                     &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(Value::LIST, result.type());
  ASSERT_EQ(3u, result.list_value().size());
  EXPECT_EQ("",    result.list_value()[0].string_value());
  EXPECT_EQ("foo", result.list_value()[1].string_value());
  EXPECT_EQ("bar", result.list_value()[2].string_value());
}

TEST(InputConversion, ValueString) {
  Err err;
  std::string input("\"str\"");
  Value result = ConvertInputToValue(input, NULL, Value(NULL, "value"), &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(Value::STRING, result.type());
  EXPECT_EQ("str", result.string_value());
}

TEST(InputConversion, ValueInt) {
  Err err;
  std::string input("\n\n  6 \n ");
  Value result = ConvertInputToValue(input, NULL, Value(NULL, "value"), &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(Value::INTEGER, result.type());
  EXPECT_EQ(6, result.int_value());
}

TEST(InputConversion, ValueList) {
  Err err;
  std::string input("\n [ \"a\", 5]");
  Value result = ConvertInputToValue(input, NULL, Value(NULL, "value"), &err);
  EXPECT_FALSE(err.has_error());
  ASSERT_EQ(Value::LIST, result.type());
  ASSERT_EQ(2u, result.list_value().size());
  EXPECT_EQ("a", result.list_value()[0].string_value());
  EXPECT_EQ(5,   result.list_value()[1].int_value());
}

TEST(InputConversion, ValueEmpty) {
  Err err;
  ConvertInputToValue("", NULL, Value(NULL, "value"), &err);
}

TEST(InputConversion, ValueError) {
  Err err;
  std::string input("\n [ \"a\", 5\nfoo bar");
  Value result = ConvertInputToValue(input, NULL, Value(NULL, "value"), &err);
  EXPECT_TRUE(err.has_error());

  // Blocks not allowed.
  input = "{ foo = 5 }";
  result = ConvertInputToValue(input, NULL, Value(NULL, "value"), &err);
  EXPECT_TRUE(err.has_error());

  // Function calls not allowed.
  input = "print(5)";
  result = ConvertInputToValue(input, NULL, Value(NULL, "value"), &err);
  EXPECT_TRUE(err.has_error());
}
