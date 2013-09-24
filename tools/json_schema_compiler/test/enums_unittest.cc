// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/enums.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_util.h"

using namespace test::api::enums;
using json_schema_compiler::test_util::List;

TEST(JsonSchemaCompilerEnumsTest, EnumTypePopulate) {
  {
    EnumType enum_type;
    DictionaryValue value;
    value.Set("type", Value::CreateStringValue("one"));
    EXPECT_TRUE(EnumType::Populate(value, &enum_type));
    EXPECT_EQ(EnumType::TYPE_ONE, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    EnumType enum_type;
    DictionaryValue value;
    value.Set("type", Value::CreateStringValue("invalid"));
    EXPECT_FALSE(EnumType::Populate(value, &enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsAsTypes) {
  {
    ListValue args;
    args.Append(Value::CreateStringValue("one"));

    scoped_ptr<TakesEnumAsType::Params> params(
        TakesEnumAsType::Params::Create(args));
    ASSERT_TRUE(params.get());
    EXPECT_EQ(ENUMERATION_ONE, params->enumeration);

    EXPECT_TRUE(args.Equals(ReturnsEnumAsType::Results::Create(
        ENUMERATION_ONE).get()));
  }
  {
    HasEnumeration enumeration;
    DictionaryValue value;
    ASSERT_FALSE(HasEnumeration::Populate(value, &enumeration));

    value.Set("enumeration", Value::CreateStringValue("one"));
    ASSERT_TRUE(HasEnumeration::Populate(value, &enumeration));
    EXPECT_TRUE(value.Equals(enumeration.ToValue().get()));

    value.Set("optional_enumeration", Value::CreateStringValue("two"));
    ASSERT_TRUE(HasEnumeration::Populate(value, &enumeration));
    EXPECT_TRUE(value.Equals(enumeration.ToValue().get()));
  }
}

TEST(JsonSchemaCompilerEnumsTest, EnumsArrayAsType) {
  {
    ListValue params_value;
    params_value.Append(List(Value::CreateStringValue("one"),
                             Value::CreateStringValue("two")).release());
    scoped_ptr<TakesEnumArrayAsType::Params> params(
        TakesEnumArrayAsType::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(ENUMERATION_ONE, params->values[0]);
    EXPECT_EQ(ENUMERATION_TWO, params->values[1]);
  }
  {
    ListValue params_value;
    params_value.Append(List(Value::CreateStringValue("invalid")).release());
    scoped_ptr<TakesEnumArrayAsType::Params> params(
        TakesEnumArrayAsType::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsEnumCreate) {
  {
    ReturnsEnum::Results::State state = ReturnsEnum::Results::STATE_FOO;
    scoped_ptr<Value> result(
        new base::StringValue(ReturnsEnum::Results::ToString(state)));
    scoped_ptr<Value> expected(Value::CreateStringValue("foo"));
    EXPECT_TRUE(result->Equals(expected.get()));
  }
  {
    ReturnsEnum::Results::State state = ReturnsEnum::Results::STATE_FOO;
    scoped_ptr<ListValue> results = ReturnsEnum::Results::Create(state);
    ListValue expected;
    expected.Append(Value::CreateStringValue("foo"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, ReturnsTwoEnumsCreate) {
  {
    scoped_ptr<ListValue> results = ReturnsTwoEnums::Results::Create(
        ReturnsTwoEnums::Results::FIRST_STATE_FOO,
        ReturnsTwoEnums::Results::SECOND_STATE_HAM);
    ListValue expected;
    expected.Append(Value::CreateStringValue("foo"));
    expected.Append(Value::CreateStringValue("ham"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, OptionalEnumTypePopulate) {
  {
    OptionalEnumType enum_type;
    DictionaryValue value;
    value.Set("type", Value::CreateStringValue("two"));
    EXPECT_TRUE(OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(OptionalEnumType::TYPE_TWO, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    OptionalEnumType enum_type;
    DictionaryValue value;
    EXPECT_TRUE(OptionalEnumType::Populate(value, &enum_type));
    EXPECT_EQ(OptionalEnumType::TYPE_NONE, enum_type.type);
    EXPECT_TRUE(value.Equals(enum_type.ToValue().get()));
  }
  {
    OptionalEnumType enum_type;
    DictionaryValue value;
    value.Set("type", Value::CreateStringValue("invalid"));
    EXPECT_FALSE(OptionalEnumType::Populate(value, &enum_type));
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumParamsCreate) {
  {
    ListValue params_value;
    params_value.Append(Value::CreateStringValue("baz"));
    scoped_ptr<TakesEnum::Params> params(
        TakesEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesEnum::Params::STATE_BAZ, params->state);
  }
  {
    ListValue params_value;
    params_value.Append(Value::CreateStringValue("invalid"));
    scoped_ptr<TakesEnum::Params> params(
        TakesEnum::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesEnumArrayParamsCreate) {
  {
    ListValue params_value;
    params_value.Append(List(Value::CreateStringValue("foo"),
                             Value::CreateStringValue("bar")).release());
    scoped_ptr<TakesEnumArray::Params> params(
        TakesEnumArray::Params::Create(params_value));
    ASSERT_TRUE(params);
    EXPECT_EQ(2U, params->values.size());
    EXPECT_EQ(TakesEnumArray::Params::VALUES_TYPE_FOO, params->values[0]);
    EXPECT_EQ(TakesEnumArray::Params::VALUES_TYPE_BAR, params->values[1]);
  }
  {
    ListValue params_value;
    params_value.Append(List(Value::CreateStringValue("invalid")).release());
    scoped_ptr<TakesEnumArray::Params> params(
        TakesEnumArray::Params::Create(params_value));
    EXPECT_FALSE(params);
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesOptionalEnumParamsCreate) {
  {
    ListValue params_value;
    params_value.Append(Value::CreateStringValue("baz"));
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesOptionalEnum::Params::STATE_BAZ, params->state);
  }
  {
    ListValue params_value;
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesOptionalEnum::Params::STATE_NONE, params->state);
  }
  {
    ListValue params_value;
    params_value.Append(Value::CreateStringValue("invalid"));
    scoped_ptr<TakesOptionalEnum::Params> params(
        TakesOptionalEnum::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, TakesMultipleOptionalEnumsParamsCreate) {
  {
    ListValue params_value;
    params_value.Append(Value::CreateStringValue("foo"));
    params_value.Append(Value::CreateStringValue("foo"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_FOO, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_FOO, params->type);
  }
  {
    ListValue params_value;
    params_value.Append(Value::CreateStringValue("foo"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_FOO, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_NONE, params->type);
  }
  {
    ListValue params_value;
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_TRUE(params.get());
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::STATE_NONE, params->state);
    EXPECT_EQ(TakesMultipleOptionalEnums::Params::TYPE_NONE, params->type);
  }
  {
    ListValue params_value;
    params_value.Append(Value::CreateStringValue("baz"));
    params_value.Append(Value::CreateStringValue("invalid"));
    scoped_ptr<TakesMultipleOptionalEnums::Params> params(
        TakesMultipleOptionalEnums::Params::Create(params_value));
    EXPECT_FALSE(params.get());
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnEnumFiredCreate) {
  {
    OnEnumFired::SomeEnum some_enum = OnEnumFired::SOME_ENUM_FOO;
    scoped_ptr<Value> result(
        new base::StringValue(OnEnumFired::ToString(some_enum)));
    scoped_ptr<Value> expected(Value::CreateStringValue("foo"));
    EXPECT_TRUE(result->Equals(expected.get()));
  }
  {
    OnEnumFired::SomeEnum some_enum = OnEnumFired::SOME_ENUM_FOO;
    scoped_ptr<ListValue> results(OnEnumFired::Create(some_enum));
    ListValue expected;
    expected.Append(Value::CreateStringValue("foo"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}

TEST(JsonSchemaCompilerEnumsTest, OnTwoEnumsFiredCreate) {
  {
    scoped_ptr<Value> results(OnTwoEnumsFired::Create(
        OnTwoEnumsFired::FIRST_ENUM_FOO,
        OnTwoEnumsFired::SECOND_ENUM_HAM));
    ListValue expected;
    expected.Append(Value::CreateStringValue("foo"));
    expected.Append(Value::CreateStringValue("ham"));
    EXPECT_TRUE(results->Equals(&expected));
  }
}
