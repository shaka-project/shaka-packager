// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/arrays.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::arrays;

namespace {

// TODO(calamity): Change to AppendString etc once kalman's patch goes through
static scoped_ptr<DictionaryValue> CreateBasicArrayTypeDictionary() {
  DictionaryValue* value = new DictionaryValue();
  ListValue* strings_value = new ListValue();
  strings_value->Append(Value::CreateStringValue("a"));
  strings_value->Append(Value::CreateStringValue("b"));
  strings_value->Append(Value::CreateStringValue("c"));
  strings_value->Append(Value::CreateStringValue("it's easy as"));
  ListValue* integers_value = new ListValue();
  integers_value->Append(Value::CreateIntegerValue(1));
  integers_value->Append(Value::CreateIntegerValue(2));
  integers_value->Append(Value::CreateIntegerValue(3));
  ListValue* booleans_value = new ListValue();
  booleans_value->Append(Value::CreateBooleanValue(false));
  booleans_value->Append(Value::CreateBooleanValue(true));
  ListValue* numbers_value = new ListValue();
  numbers_value->Append(Value::CreateDoubleValue(6.1));
  value->Set("numbers", numbers_value);
  value->Set("booleans", booleans_value);
  value->Set("strings", strings_value);
  value->Set("integers", integers_value);
  return scoped_ptr<DictionaryValue>(value);
}

static Value* CreateItemValue(int val) {
  DictionaryValue* value(new DictionaryValue());
  value->Set("val", Value::CreateIntegerValue(val));
  return value;
}

}  // namespace

TEST(JsonSchemaCompilerArrayTest, BasicArrayType) {
  {
    scoped_ptr<DictionaryValue> value = CreateBasicArrayTypeDictionary();
    scoped_ptr<BasicArrayType> basic_array_type(new BasicArrayType());
    ASSERT_TRUE(BasicArrayType::Populate(*value, basic_array_type.get()));
    EXPECT_TRUE(value->Equals(basic_array_type->ToValue().get()));
  }
}

TEST(JsonSchemaCompilerArrayTest, EnumArrayType) {
  std::vector<EnumArrayType::TypesType> enums;
  enums.push_back(EnumArrayType::TYPES_TYPE_ONE);
  enums.push_back(EnumArrayType::TYPES_TYPE_TWO);
  enums.push_back(EnumArrayType::TYPES_TYPE_THREE);

  scoped_ptr<ListValue> types(new ListValue());
  for (size_t i = 0; i < enums.size(); ++i)
    types->Append(new base::StringValue(EnumArrayType::ToString(enums[i])));

  DictionaryValue value;
  value.Set("types", types.release());

  EnumArrayType enum_array_type;
  ASSERT_TRUE(EnumArrayType::Populate(value, &enum_array_type));
  EXPECT_EQ(enums, enum_array_type.types);
}

TEST(JsonSchemaCompilerArrayTest, OptionalEnumArrayType) {
  {
    std::vector<OptionalEnumArrayType::TypesType> enums;
    enums.push_back(OptionalEnumArrayType::TYPES_TYPE_ONE);
    enums.push_back(OptionalEnumArrayType::TYPES_TYPE_TWO);
    enums.push_back(OptionalEnumArrayType::TYPES_TYPE_THREE);

    scoped_ptr<ListValue> types(new ListValue());
    for (size_t i = 0; i < enums.size(); ++i) {
      types->Append(new base::StringValue(
          OptionalEnumArrayType::ToString(enums[i])));
    }

    DictionaryValue value;
    value.Set("types", types.release());

    OptionalEnumArrayType enum_array_type;
    ASSERT_TRUE(OptionalEnumArrayType::Populate(value, &enum_array_type));
    EXPECT_EQ(enums, *enum_array_type.types);
  }
  {
    DictionaryValue value;
    scoped_ptr<ListValue> enum_array(new ListValue());
    enum_array->Append(Value::CreateStringValue("invalid"));

    value.Set("types", enum_array.release());
    OptionalEnumArrayType enum_array_type;
    ASSERT_FALSE(OptionalEnumArrayType::Populate(value, &enum_array_type));
    EXPECT_TRUE(enum_array_type.types->empty());
  }
}

TEST(JsonSchemaCompilerArrayTest, RefArrayType) {
  {
    scoped_ptr<DictionaryValue> value(new DictionaryValue());
    scoped_ptr<ListValue> ref_array(new ListValue());
    ref_array->Append(CreateItemValue(1));
    ref_array->Append(CreateItemValue(2));
    ref_array->Append(CreateItemValue(3));
    value->Set("refs", ref_array.release());
    scoped_ptr<RefArrayType> ref_array_type(new RefArrayType());
    EXPECT_TRUE(RefArrayType::Populate(*value, ref_array_type.get()));
    ASSERT_EQ(3u, ref_array_type->refs.size());
    EXPECT_EQ(1, ref_array_type->refs[0]->val);
    EXPECT_EQ(2, ref_array_type->refs[1]->val);
    EXPECT_EQ(3, ref_array_type->refs[2]->val);
  }
  {
    scoped_ptr<DictionaryValue> value(new DictionaryValue());
    scoped_ptr<ListValue> not_ref_array(new ListValue());
    not_ref_array->Append(CreateItemValue(1));
    not_ref_array->Append(Value::CreateIntegerValue(3));
    value->Set("refs", not_ref_array.release());
    scoped_ptr<RefArrayType> ref_array_type(new RefArrayType());
    EXPECT_FALSE(RefArrayType::Populate(*value, ref_array_type.get()));
  }
}

TEST(JsonSchemaCompilerArrayTest, IntegerArrayParamsCreate) {
  scoped_ptr<ListValue> params_value(new ListValue());
  scoped_ptr<ListValue> integer_array(new ListValue());
  integer_array->Append(Value::CreateIntegerValue(2));
  integer_array->Append(Value::CreateIntegerValue(4));
  integer_array->Append(Value::CreateIntegerValue(8));
  params_value->Append(integer_array.release());
  scoped_ptr<IntegerArray::Params> params(
      IntegerArray::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(3u, params->nums.size());
  EXPECT_EQ(2, params->nums[0]);
  EXPECT_EQ(4, params->nums[1]);
  EXPECT_EQ(8, params->nums[2]);
}

TEST(JsonSchemaCompilerArrayTest, AnyArrayParamsCreate) {
  scoped_ptr<ListValue> params_value(new ListValue());
  scoped_ptr<ListValue> any_array(new ListValue());
  any_array->Append(Value::CreateIntegerValue(1));
  any_array->Append(Value::CreateStringValue("test"));
  any_array->Append(CreateItemValue(2));
  params_value->Append(any_array.release());
  scoped_ptr<AnyArray::Params> params(
      AnyArray::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(3u, params->anys.size());
  int int_temp = 0;
  EXPECT_TRUE(params->anys[0]->GetAsInteger(&int_temp));
  EXPECT_EQ(1, int_temp);
}

TEST(JsonSchemaCompilerArrayTest, ObjectArrayParamsCreate) {
  scoped_ptr<ListValue> params_value(new ListValue());
  scoped_ptr<ListValue> item_array(new ListValue());
  item_array->Append(CreateItemValue(1));
  item_array->Append(CreateItemValue(2));
  params_value->Append(item_array.release());
  scoped_ptr<ObjectArray::Params> params(
      ObjectArray::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(2u, params->objects.size());
  EXPECT_EQ(1, params->objects[0]->additional_properties["val"]);
  EXPECT_EQ(2, params->objects[1]->additional_properties["val"]);
}

TEST(JsonSchemaCompilerArrayTest, RefArrayParamsCreate) {
  scoped_ptr<ListValue> params_value(new ListValue());
  scoped_ptr<ListValue> item_array(new ListValue());
  item_array->Append(CreateItemValue(1));
  item_array->Append(CreateItemValue(2));
  params_value->Append(item_array.release());
  scoped_ptr<RefArray::Params> params(
      RefArray::Params::Create(*params_value));
  EXPECT_TRUE(params.get());
  ASSERT_EQ(2u, params->refs.size());
  EXPECT_EQ(1, params->refs[0]->val);
  EXPECT_EQ(2, params->refs[1]->val);
}

TEST(JsonSchemaCompilerArrayTest, ReturnIntegerArrayResultCreate) {
  std::vector<int> integers;
  integers.push_back(1);
  integers.push_back(2);
  scoped_ptr<ListValue> results = ReturnIntegerArray::Results::Create(integers);

  ListValue expected;
  ListValue* expected_argument = new ListValue();
  expected_argument->Append(Value::CreateIntegerValue(1));
  expected_argument->Append(Value::CreateIntegerValue(2));
  expected.Append(expected_argument);
  EXPECT_TRUE(results->Equals(&expected));
}

TEST(JsonSchemaCompilerArrayTest, ReturnRefArrayResultCreate) {
  std::vector<linked_ptr<Item> > items;
  items.push_back(linked_ptr<Item>(new Item()));
  items.push_back(linked_ptr<Item>(new Item()));
  items[0]->val = 1;
  items[1]->val = 2;
  scoped_ptr<ListValue> results = ReturnRefArray::Results::Create(items);

  ListValue expected;
  ListValue* expected_argument = new ListValue();
  DictionaryValue* first = new DictionaryValue();
  first->SetInteger("val", 1);
  expected_argument->Append(first);
  DictionaryValue* second = new DictionaryValue();
  second->SetInteger("val", 2);
  expected_argument->Append(second);
  expected.Append(expected_argument);
  EXPECT_TRUE(results->Equals(&expected));
}
