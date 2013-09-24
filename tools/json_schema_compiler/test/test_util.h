// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_JSON_SCHEMA_COMPILER_TEST_TEST_UTIL_H_
#define TOOLS_JSON_SCHEMA_COMPILER_TEST_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/values.h"

namespace json_schema_compiler {
namespace test_util {

scoped_ptr<base::Value> ReadJson(const base::StringPiece& json);

template <typename T>
std::vector<T> Vector(const T& a) {
  std::vector<T> arr;
  arr.push_back(a);
  return arr;
}
template <typename T>
std::vector<T> Vector(const T& a, const T& b) {
  std::vector<T> arr = Vector(a);
  arr.push_back(b);
  return arr;
}
template <typename T>
std::vector<T> Vector(const T& a, const T& b, const T& c) {
  std::vector<T> arr = Vector(a, b);
  arr.push_back(c);
  return arr;
}

scoped_ptr<base::ListValue> List(base::Value* a);
scoped_ptr<base::ListValue> List(base::Value* a, base::Value* b);
scoped_ptr<base::ListValue> List(base::Value* a,
                                 base::Value* b,
                                 base::Value* c);

scoped_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak, base::Value* av);
scoped_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak, base::Value* av,
    const std::string& bk, base::Value* bv);
scoped_ptr<base::DictionaryValue> Dictionary(
    const std::string& ak, base::Value* av,
    const std::string& bk, base::Value* bv,
    const std::string& ck, base::Value* cv);

}  // namespace test_util
}  // namespace json_schema_compiler

#endif  // TOOLS_JSON_SCHEMA_COMPILER_TEST_TEST_UTIL_H_
