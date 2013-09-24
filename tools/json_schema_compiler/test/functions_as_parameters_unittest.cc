// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/functions_as_parameters.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::functions_as_parameters;

TEST(JsonSchemaCompilerFunctionsAsParametersTest, PopulateRequiredFunction) {
  // The expectation is that if any value is set for the function, then
  // the function is "present".
  {
    DictionaryValue empty_value;
    FunctionType out;
    EXPECT_FALSE(FunctionType::Populate(empty_value, &out));
  }
  {
    DictionaryValue value;
    DictionaryValue function_dict;
    value.Set("event_callback", function_dict.DeepCopy());
    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, &out));
    EXPECT_TRUE(out.event_callback.empty());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, RequiredFunctionToValue) {
  {
    DictionaryValue value;
    DictionaryValue function_dict;
    value.Set("event_callback", function_dict.DeepCopy());

    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, &out));
    EXPECT_TRUE(value.Equals(out.ToValue().get()));
  }
  {
    DictionaryValue value;
    DictionaryValue expected_value;
    DictionaryValue function_dict;
    value.Set("event_callback", function_dict.DeepCopy());
    expected_value.Set("event_callback", function_dict.DeepCopy());

    FunctionType out;
    ASSERT_TRUE(FunctionType::Populate(value, &out));
    EXPECT_TRUE(expected_value.Equals(out.ToValue().get()));
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, PopulateOptionalFunction) {
  {
    DictionaryValue empty_value;
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(empty_value, &out));
    EXPECT_FALSE(out.event_callback.get());
  }
  {
    DictionaryValue value;
    DictionaryValue function_value;
    value.Set("event_callback", function_value.DeepCopy());
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, &out));
    EXPECT_TRUE(out.event_callback.get());
  }
  {
    DictionaryValue value;
    DictionaryValue function_value;
    value.Set("event_callback", function_value.DeepCopy());
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, &out));
    EXPECT_TRUE(out.event_callback.get());
  }
}

TEST(JsonSchemaCompilerFunctionsAsParametersTest, OptionalFunctionToValue) {
  {
    DictionaryValue empty_value;
    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(empty_value, &out));
    // event_callback should not be set in the return from ToValue.
    EXPECT_TRUE(empty_value.Equals(out.ToValue().get()));
  }
  {
    DictionaryValue value;
    DictionaryValue function_value;
    value.Set("event_callback", function_value.DeepCopy());

    OptionalFunctionType out;
    ASSERT_TRUE(OptionalFunctionType::Populate(value, &out));
    EXPECT_TRUE(value.Equals(out.ToValue().get()));
  }
}
