// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TOKEN_H_
#define TOOLS_GN_TOKEN_H_

#include "base/strings/string_piece.h"
#include "tools/gn/location.h"

class Token {
 public:
  enum Type {
    INVALID,
    INTEGER,    // 123
    STRING,     // "blah"
    OPERATOR,   // =, +=, -=, +, -, ==, !=, <=, >=, <, >
    IDENTIFIER, // foo
    SCOPER,     // (, ), [, ], {, }
    SEPARATOR,  // ,
    COMMENT     // #...\n
  };

  Token();
  Token(const Location& location, Type t, const base::StringPiece& v);

  Type type() const { return type_; }
  const base::StringPiece& value() const { return value_; }
  const Location& location() const { return location_; }
  LocationRange range() const {
    return LocationRange(location_,
                         Location(location_.file(), location_.line_number(),
                                  location_.char_offset() + value_.size()));
  }

  // Helper functions for comparing this token to something.
  bool IsIdentifierEqualTo(const char* v) const;
  bool IsOperatorEqualTo(const char* v) const;
  bool IsScoperEqualTo(const char* v) const;
  bool IsStringEqualTo(const char* v) const;

  // For STRING tokens, returns the string value (no quotes at end, does
  // unescaping).
  std::string StringValue() const;

 private:
  Type type_;
  base::StringPiece value_;
  Location location_;
};

#endif  // TOOLS_GN_TOKEN_H_
