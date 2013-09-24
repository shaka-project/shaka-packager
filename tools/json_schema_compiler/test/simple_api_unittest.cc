// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/simple_api.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::simple_api;

namespace {

static scoped_ptr<DictionaryValue> CreateTestTypeDictionary() {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetWithoutPathExpansion("number", Value::CreateDoubleValue(1.1));
  value->SetWithoutPathExpansion("integer", Value::CreateIntegerValue(4));
  value->SetWithoutPathExpansion("string", Value::CreateStringValue("bling"));
  value->SetWithoutPathExpansion("boolean", Value::CreateBooleanValue(true));
  return value.Pass();
}

}  // namespace

TEST(JsonSchemaCompilerSimpleTest, IncrementIntegerResultCreate) {
  scoped_ptr<ListValue> results = IncrementInteger::Results::Create(5);
  ListValue expected;
  expected.Append(Value::CreateIntegerValue(5));
  EXPECT_TRUE(results->Equals(&expected));
}

TEST(JsonSchemaCompilerSimpleTest, IncrementIntegerParamsCreate) {
  scoped_ptr<ListValue> params_value(new ListValue());
  params_value->Append(Value::CreateIntegerValue(6));
  scoped_ptr<IncrementInteger::Params> params(
      IncrementInteger::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_EQ(6, params->num);
}

TEST(JsonSchemaCompilerSimpleTest, NumberOfParams) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("text"));
    params_value->Append(Value::CreateStringValue("text"));
    scoped_ptr<OptionalString::Params> params(
        OptionalString::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<IncrementInteger::Params> params(
        IncrementInteger::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalStringParamsCreate) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<OptionalString::Params> params(
        OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->str.get());
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("asdf"));
    scoped_ptr<OptionalString::Params> params(
        OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_TRUE(params->str.get());
    EXPECT_EQ("asdf", *params->str);
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalParamsTakingNull) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateNullValue());
    scoped_ptr<OptionalString::Params> params(
        OptionalString::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->str.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalStringParamsWrongType) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateIntegerValue(5));
    scoped_ptr<OptionalString::Params> params(
        OptionalString::Params::Create(*params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerSimpleTest, OptionalBeforeRequired) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateNullValue());
    params_value->Append(Value::CreateStringValue("asdf"));
    scoped_ptr<OptionalBeforeRequired::Params> params(
        OptionalBeforeRequired::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->first.get());
    EXPECT_EQ("asdf", params->second);
  }
}

TEST(JsonSchemaCompilerSimpleTest, NoParamsResultCreate) {
  scoped_ptr<ListValue> results = OptionalString::Results::Create();
  ListValue expected;
  EXPECT_TRUE(results->Equals(&expected));
}

TEST(JsonSchemaCompilerSimpleTest, TestTypePopulate) {
  {
    scoped_ptr<TestType> test_type(new TestType());
    scoped_ptr<DictionaryValue> value = CreateTestTypeDictionary();
    EXPECT_TRUE(TestType::Populate(*value, test_type.get()));
    EXPECT_EQ("bling", test_type->string);
    EXPECT_EQ(1.1, test_type->number);
    EXPECT_EQ(4, test_type->integer);
    EXPECT_EQ(true, test_type->boolean);
    EXPECT_TRUE(value->Equals(test_type->ToValue().get()));
  }
  {
    scoped_ptr<TestType> test_type(new TestType());
    scoped_ptr<DictionaryValue> value = CreateTestTypeDictionary();
    value->Remove("number", NULL);
    EXPECT_FALSE(TestType::Populate(*value, test_type.get()));
  }
}

TEST(JsonSchemaCompilerSimpleTest, GetTestType) {
  {
    scoped_ptr<DictionaryValue> value = CreateTestTypeDictionary();
    scoped_ptr<TestType> test_type(new TestType());
    EXPECT_TRUE(TestType::Populate(*value, test_type.get()));
    scoped_ptr<ListValue> results = GetTestType::Results::Create(*test_type);

    DictionaryValue* result = NULL;
    results->GetDictionary(0, &result);
    EXPECT_TRUE(result->Equals(value.get()));
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnIntegerFiredCreate) {
  {
    scoped_ptr<ListValue> results(OnIntegerFired::Create(5));
    ListValue expected;
    expected.Append(Value::CreateIntegerValue(5));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnStringFiredCreate) {
  {
    scoped_ptr<ListValue> results(OnStringFired::Create("yo dawg"));
    ListValue expected;
    expected.Append(Value::CreateStringValue("yo dawg"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerSimpleTest, OnTestTypeFiredCreate) {
  {
    TestType some_test_type;
    scoped_ptr<DictionaryValue> expected = CreateTestTypeDictionary();
    ASSERT_TRUE(expected->GetDouble("number", &some_test_type.number));
    ASSERT_TRUE(expected->GetString("string", &some_test_type.string));
    ASSERT_TRUE(expected->GetInteger("integer", &some_test_type.integer));
    ASSERT_TRUE(expected->GetBoolean("boolean", &some_test_type.boolean));

    scoped_ptr<ListValue> results(OnTestTypeFired::Create(some_test_type));
    DictionaryValue* result = NULL;
    results->GetDictionary(0, &result);
    EXPECT_TRUE(result->Equals(expected.get()));
  }
}
