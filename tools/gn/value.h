// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VALUE_H_
#define TOOLS_GN_VALUE_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "tools/gn/err.h"

class ParseNode;

// Represents a variable value in the interpreter.
class Value {
 public:
  enum Type {
    NONE = 0,
    INTEGER,
    STRING,
    LIST
  };

  Value();
  Value(const ParseNode* origin, Type t);
  Value(const ParseNode* origin, int64 int_val);
  Value(const ParseNode* origin, const base::StringPiece& str_val);
  ~Value();

  Type type() const { return type_; }

  // Returns a string describing the given type.
  static const char* DescribeType(Type t);

  // Returns the node that made this. May be NULL.
  const ParseNode* origin() const { return origin_; }
  void set_origin(const ParseNode* o) { origin_ = o; }

  int64& int_value() {
    DCHECK(type_ == INTEGER);
    return int_value_;
  }
  const int64& int_value() const {
    DCHECK(type_ == INTEGER);
    return int_value_;
  }

  std::string& string_value() {
    DCHECK(type_ == STRING);
    return string_value_;
  }
  const std::string& string_value() const {
    DCHECK(type_ == STRING);
    return string_value_;
  }

  std::vector<Value>& list_value() {
    DCHECK(type_ == LIST);
    return list_value_;
  }
  const std::vector<Value>& list_value() const {
    DCHECK(type_ == LIST);
    return list_value_;
  }

  // Returns the current value converted to an int, normally used for
  // boolean operations. Undefined variables, empty lists, and empty strings
  // are all interpreted as 0, otherwise 1.
  int64 InterpretAsInt() const;

  // Converts the given value to a string.
  std::string ToString() const;

  // Verifies that the value is of the given type. If it isn't, returns
  // false and sets the error.
  bool VerifyTypeIs(Type t, Err* err) const;

  // Compares values. Only the "value" is compared, not the origin.
  bool operator==(const Value& other) const;
  bool operator!=(const Value& other) const;

 private:
  Type type_;
  std::string string_value_;
  int64 int_value_;
  std::vector<Value> list_value_;
  const ParseNode* origin_;
};

#endif  // TOOLS_GN_VALUE_H_
