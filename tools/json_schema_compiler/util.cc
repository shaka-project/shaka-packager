// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/util.h"

#include "base/values.h"

namespace json_schema_compiler {
namespace util {

bool GetItemFromList(const base::ListValue& from, int index, int* out) {
  return from.GetInteger(index, out);
}

bool GetItemFromList(const base::ListValue& from, int index, bool* out) {
  return from.GetBoolean(index, out);
}

bool GetItemFromList(const base::ListValue& from, int index, double* out) {
  return from.GetDouble(index, out);
}

bool GetItemFromList(const base::ListValue& from, int index, std::string* out) {
  return from.GetString(index, out);
}

bool GetItemFromList(const base::ListValue& from,
                     int index,
                     linked_ptr<base::Value>* out) {
  const base::Value* value = NULL;
  if (!from.Get(index, &value))
    return false;
  *out = make_linked_ptr(value->DeepCopy());
  return true;
}

bool GetItemFromList(const base::ListValue& from, int index,
    linked_ptr<base::DictionaryValue>* out) {
  const base::DictionaryValue* dict = NULL;
  if (!from.GetDictionary(index, &dict))
    return false;
  *out = make_linked_ptr(dict->DeepCopy());
  return true;
}

void AddItemToList(const int from, base::ListValue* out) {
  out->Append(new base::FundamentalValue(from));
}

void AddItemToList(const bool from, base::ListValue* out) {
  out->Append(new base::FundamentalValue(from));
}

void AddItemToList(const double from, base::ListValue* out) {
  out->Append(new base::FundamentalValue(from));
}

void AddItemToList(const std::string& from, base::ListValue* out) {
  out->Append(new base::StringValue(from));
}

void AddItemToList(const linked_ptr<base::Value>& from,
                   base::ListValue* out) {
  out->Append(from->DeepCopy());
}

void AddItemToList(const linked_ptr<base::DictionaryValue>& from,
                   base::ListValue* out) {
  out->Append(static_cast<base::Value*>(from->DeepCopy()));
}

std::string ValueTypeToString(Value::Type type) {
  switch(type) {
    case Value::TYPE_NULL:
      return "null";
    case Value::TYPE_BOOLEAN:
      return "boolean";
    case Value::TYPE_INTEGER:
      return "integer";
    case Value::TYPE_DOUBLE:
      return "number";
    case Value::TYPE_STRING:
      return "string";
    case Value::TYPE_BINARY:
      return "binary";
    case Value::TYPE_DICTIONARY:
      return "dictionary";
    case Value::TYPE_LIST:
      return "list";
  }
  NOTREACHED();
  return "";
}

}  // namespace api_util
}  // namespace extensions
