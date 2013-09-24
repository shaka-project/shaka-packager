// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/token.h"

#include "base/logging.h"

namespace {

std::string UnescapeString(const base::StringPiece& input) {
  std::string result;
  result.reserve(input.size());

  for (size_t i = 0; i < input.size(); i++) {
    if (input[i] == '\\') {
      DCHECK(i < input.size() - 1);  // Last char shouldn't be a backslash or
                                     // it would have escaped the terminator.
      i++;  // Skip backslash, next char is a literal.
    }
    result.push_back(input[i]);
  }
  return result;
}

}  // namespace

Token::Token() : type_(INVALID), value_() {
}

Token::Token(const Location& location,
             Type t,
             const base::StringPiece& v)
    : type_(t),
      value_(v),
      location_(location) {
}

bool Token::IsIdentifierEqualTo(const char* v) const {
  return type_ == IDENTIFIER && value_ == v;
}

bool Token::IsOperatorEqualTo(const char* v) const {
  return type_ == OPERATOR && value_ == v;
}

bool Token::IsScoperEqualTo(const char* v) const {
  return type_ == SCOPER && value_ == v;
}

bool Token::IsStringEqualTo(const char* v) const {
  return type_ == STRING && value_ == v;
}

std::string Token::StringValue() const {
  DCHECK(type() == STRING);

  // Trim off the string terminators at the end.
  return UnescapeString(value_.substr(1, value_.size() - 2));
}
