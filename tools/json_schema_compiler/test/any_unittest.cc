// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/any.h"

using namespace test::api::any;

TEST(JsonSchemaCompilerAnyTest, AnyTypePopulate) {
  {
    AnyType any_type;
    scoped_ptr<DictionaryValue> any_type_value(new DictionaryValue());
    any_type_value->SetString("any", "value");
    EXPECT_TRUE(AnyType::Populate(*any_type_value, &any_type));
    scoped_ptr<Value> any_type_to_value(any_type.ToValue());
    EXPECT_TRUE(any_type_value->Equals(any_type_to_value.get()));
  }
  {
    AnyType any_type;
    scoped_ptr<DictionaryValue> any_type_value(new DictionaryValue());
    any_type_value->SetInteger("any", 5);
    EXPECT_TRUE(AnyType::Populate(*any_type_value, &any_type));
    scoped_ptr<Value> any_type_to_value(any_type.ToValue());
    EXPECT_TRUE(any_type_value->Equals(any_type_to_value.get()));
  }
}

TEST(JsonSchemaCompilerAnyTest, OptionalAnyParamsCreate) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<OptionalAny::Params> params(
        OptionalAny::Params::Create(*params_value));
    EXPECT_TRUE(params.get());
    EXPECT_FALSE(params->any_name.get());
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<Value> param(Value::CreateStringValue("asdf"));
    params_value->Append(param->DeepCopy());
    scoped_ptr<OptionalAny::Params> params(
        OptionalAny::Params::Create(*params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->any_name);
    EXPECT_TRUE(params->any_name->Equals(param.get()));
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<Value> param(Value::CreateBooleanValue(true));
    params_value->Append(param->DeepCopy());
    scoped_ptr<OptionalAny::Params> params(
        OptionalAny::Params::Create(*params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->any_name);
    EXPECT_TRUE(params->any_name->Equals(param.get()));
  }
}
