// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/test/test_util.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/logging.h"

namespace json_schema_compiler {
namespace test_util {

scoped_ptr<base::Value> ReadJson(const base::StringPiece& json) {
  int error_code;
  std::string error_msg;
  scoped_ptr<base::Value> result(base::JSONReader::ReadAndReturnError(
      json,
      base::JSON_ALLOW_TRAILING_COMMAS,
      &error_code,
      &error_msg));
  // CHECK not ASSERT since passing invalid |json| is a test error.
  CHECK(result) << error_msg;
  return result.Pass();
}

scoped_ptr<base::ListValue> List(base::Value* a) {
  scoped_ptr<base::ListValue> list(new base::ListValue());
  list->Append(a);
  return list.Pass();
}
scoped_ptr<base::ListValue> List(base::Value* a, base::Value* b) {
  scoped_ptr<base::ListValue> list = List(a);
  list->Append(b);
  return list.Pass();
}
scoped_ptr<base::ListValue> List(base::Value* a,
                                 base::Value* b,
                                 base::Value* c) {
  scoped_ptr<base::ListValue> list = List(a, b);
  list->Append(c);
  return list.Pass();
}

scoped_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak, base::Value* av) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetWithoutPathExpansion(ak, av);
  return dict.Pass();
}
scoped_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak, base::Value* av,
    const std::string& bk, base::Value* bv) {
  scoped_ptr<base::DictionaryValue> dict = Dictionary(ak, av);
  dict->SetWithoutPathExpansion(bk, bv);
  return dict.Pass();
}
scoped_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak, base::Value* av,
    const std::string& bk, base::Value* bv,
    const std::string& ck, base::Value* cv) {
  scoped_ptr<base::DictionaryValue> dict = Dictionary(ak, av, bk, bv);
  dict->SetWithoutPathExpansion(ck, cv);
  return dict.Pass();
}

}  // namespace test_util
}  // namespace json_schema_compiler
