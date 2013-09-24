// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_STRING_UTILS_H_
#define TOOLS_GN_STRING_UTILS_H_

#include "base/strings/string_piece.h"

class Err;
class Scope;
class Token;
class Value;

inline std::string operator+(const std::string& a, const base::StringPiece& b) {
  std::string ret;
  ret.reserve(a.size() + b.size());
  ret.assign(a);
  ret.append(b.data(), b.size());
  return ret;
}

inline std::string operator+(const base::StringPiece& a, const std::string& b) {
  std::string ret;
  ret.reserve(a.size() + b.size());
  ret.assign(a.data(), a.size());
  ret.append(b);
  return ret;
}

// Unescapes and expands variables in the given literal, writing the result
// to the given value. On error, sets |err| and returns false.
bool ExpandStringLiteral(Scope* scope,
                         const Token& literal,
                         Value* result,
                         Err* err);

// Removes the given prefix from the string. Asserts if the string does
// not have the given prefix.
//
// Note: could potentially return a StringPiece into the str.
std::string RemovePrefix(const std::string& str, const std::string& prefix);

// Appends the given string piece to the given string. This avoids an
// intermediate copy.
inline void AppendStringPiece(std::string* dest,
                              const base::StringPiece& piece) {
  dest->append(piece.data(), piece.size());
}

#endif  // TOOLS_GN_STRING_UTILS_H_
