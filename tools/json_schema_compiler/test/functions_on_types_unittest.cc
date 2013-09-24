// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/functions_on_types.h"

using namespace test::api::functions_on_types;

TEST(JsonSchemaCompilerFunctionsOnTypesTest, StorageAreaGetParamsCreate) {
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    scoped_ptr<StorageArea::Get::Params> params(
        StorageArea::Get::Params::Create(*params_value));
    ASSERT_TRUE(params);
    EXPECT_FALSE(params->keys);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateIntegerValue(9));
    scoped_ptr<StorageArea::Get::Params> params(
        StorageArea::Get::Params::Create(*params_value));
    EXPECT_FALSE(params);
  }
  {
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(Value::CreateStringValue("test"));
    scoped_ptr<StorageArea::Get::Params> params(
        StorageArea::Get::Params::Create(*params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->keys);
    EXPECT_EQ("test", *params->keys->as_string);
  }
  {
    scoped_ptr<DictionaryValue> keys_object_value(new DictionaryValue());
    keys_object_value->SetInteger("integer", 5);
    keys_object_value->SetString("string", "string");
    scoped_ptr<ListValue> params_value(new ListValue());
    params_value->Append(keys_object_value->DeepCopy());
    scoped_ptr<StorageArea::Get::Params> params(
        StorageArea::Get::Params::Create(*params_value));
    ASSERT_TRUE(params);
    ASSERT_TRUE(params->keys);
    EXPECT_TRUE(keys_object_value->Equals(
        &params->keys->as_object->additional_properties));
  }
}

TEST(JsonSchemaCompilerFunctionsOnTypesTest, StorageAreaGetResultCreate) {
  StorageArea::Get::Results::Items items;
  items.additional_properties.SetDouble("asdf", 0.1);
  items.additional_properties.SetString("sdfg", "zxcv");
  scoped_ptr<ListValue> results = StorageArea::Get::Results::Create(items);
  DictionaryValue* item_result = NULL;
  ASSERT_TRUE(results->GetDictionary(0, &item_result));
  EXPECT_TRUE(item_result->Equals(&items.additional_properties));
}

TEST(JsonSchemaCompilerFunctionsOnTypesTest, ChromeSettingGetParamsCreate) {
  scoped_ptr<DictionaryValue> details_value(new DictionaryValue());
  details_value->SetBoolean("incognito", true);
  scoped_ptr<ListValue> params_value(new ListValue());
  params_value->Append(details_value.release());
  scoped_ptr<ChromeSetting::Get::Params> params(
      ChromeSetting::Get::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  EXPECT_TRUE(*params->details.incognito);
}
