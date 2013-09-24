// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/callbacks.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace test::api::callbacks;

TEST(JsonSchemaCompilerCallbacksTest, ReturnsObjectResultCreate) {
  ReturnsObject::Results::SomeObject some_object;
  some_object.state = ReturnsObject::Results::SomeObject::STATE_FOO;
  scoped_ptr<ListValue> results = ReturnsObject::Results::Create(some_object);

  DictionaryValue* expected_dict = new DictionaryValue();
  expected_dict->SetString("state", "foo");
  ListValue expected;
  expected.Append(expected_dict);
  EXPECT_TRUE(results->Equals(&expected));
}

TEST(JsonSchemaCompilerCallbacksTest, ReturnsMultipleResultCreate) {
  ReturnsMultiple::Results::SomeObject some_object;
  some_object.state = ReturnsMultiple::Results::SomeObject::STATE_FOO;
  scoped_ptr<ListValue> results =
      ReturnsMultiple::Results::Create(5, some_object);

  DictionaryValue* expected_dict = new DictionaryValue();
  expected_dict->SetString("state", "foo");
  ListValue expected;
  expected.Append(Value::CreateIntegerValue(5));
  expected.Append(expected_dict);
  EXPECT_TRUE(results->Equals(&expected));
}
