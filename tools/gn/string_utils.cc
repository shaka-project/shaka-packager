// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/string_utils.h"

#include "tools/gn/err.h"
#include "tools/gn/scope.h"
#include "tools/gn/token.h"
#include "tools/gn/tokenizer.h"
#include "tools/gn/value.h"

namespace {

// Constructs an Err indicating a range inside a string. We assume that the
// token has quotes around it that are not counted by the offset.
Err ErrInsideStringToken(const Token& token, size_t offset, size_t size,
                         const std::string& msg,
                         const std::string& help = std::string()) {
  // The "+1" is skipping over the " at the beginning of the token.
  Location begin_loc(token.location().file(),
                     token.location().line_number(),
                     token.location().char_offset() + offset + 1);
  Location end_loc(token.location().file(),
                   token.location().line_number(),
                   token.location().char_offset() + offset + 1 + size);
  return Err(LocationRange(begin_loc, end_loc), msg, help);
}

// Given the character input[i] indicating the $ in a string, locates the
// identifier and places its range in |*identifier|, and updates |*i| to
// point to the last character consumed.
//
// On error returns false and sets the error.
bool LocateInlineIdenfitier(const Token& token,
                            const char* input, size_t size,
                            size_t* i,
                            base::StringPiece* identifier,
                            Err* err) {
  size_t dollars_index = *i;
  (*i)++;
  if (*i == size) {
    *err = ErrInsideStringToken(token, dollars_index, 1, "$ at end of string.",
        "I was expecting an identifier after the $.");
    return false;
  }

  bool has_brackets;
  if (input[*i] == '{') {
    (*i)++;
    if (*i == size) {
      *err = ErrInsideStringToken(token, dollars_index, 2,
          "${ at end of string.",
          "I was expecting an identifier inside the ${...}.");
      return false;
    }
    has_brackets = true;
  } else {
    has_brackets = false;
  }

  // First char is special.
  if (!Tokenizer::IsIdentifierFirstChar(input[*i])) {
    *err = ErrInsideStringToken(
        token, dollars_index, *i - dollars_index + 1,
        "$ not followed by an identifier char.",
        "It you want a literal $ use \"\\$\".");
    return false;
  }
  size_t begin_offset = *i;
  (*i)++;

  // Find the first non-identifier char following the string.
  while (*i < size && Tokenizer::IsIdentifierContinuingChar(input[*i]))
    (*i)++;
  size_t end_offset = *i;

  // If we started with a bracket, validate that there's an ending one. Leave
  // *i pointing to the last char we consumed (backing up one).
  if (has_brackets) {
    if (*i == size) {
      *err = ErrInsideStringToken(token, dollars_index, *i - dollars_index,
                                  "Unterminated ${...");
      return false;
    } else if (input[*i] != '}') {
      *err = ErrInsideStringToken(token, *i, 1, "Not an identifier in string expansion.",
          "The contents of ${...} should be an identifier. "
          "This character is out of sorts.");
      return false;
    }
    // We want to consume the bracket but also back up one, so *i is unchanged.
  } else {
    (*i)--;
  }

  *identifier = base::StringPiece(&input[begin_offset],
                                  end_offset - begin_offset);
  return true;
}

bool AppendIdentifierValue(Scope* scope,
                           const Token& token,
                           const base::StringPiece& identifier,
                           std::string* output,
                           Err* err) {
  const Value* value = scope->GetValue(identifier, true);
  if (!value) {
    // We assume the identifier points inside the token.
    *err = ErrInsideStringToken(
        token, identifier.data() - token.value().data() - 1, identifier.size(),
        "Undefined identifier in string expansion.",
        std::string("\"") + identifier + "\" is not currently in scope.");
    return false;
  }

  output->append(value->ToString());
  return true;
}

}  // namespace

bool ExpandStringLiteral(Scope* scope,
                         const Token& literal,
                         Value* result,
                         Err* err) {
  DCHECK(literal.type() == Token::STRING);
  DCHECK(literal.value().size() > 1);  // Should include quotes.
  DCHECK(result->type() == Value::STRING);  // Should be already set.

  // The token includes the surrounding quotes, so strip those off.
  const char* input = &literal.value().data()[1];
  size_t size = literal.value().size() - 2;

  std::string& output = result->string_value();
  output.reserve(size);
  for (size_t i = 0; i < size; i++) {
    if (input[i] == '\\') {
      if (i < size - 1) {
        switch (input[i + 1]) {
          case '\\':
          case '"':
          case '$':
            output.push_back(input[i + 1]);
            i++;
            continue;
          default:  // Everything else has no meaning: pass the literal.
            break;
        }
      }
      output.push_back(input[i]);
    } else if (input[i] == '$') {
      base::StringPiece identifier;
      if (!LocateInlineIdenfitier(literal, input, size, &i, &identifier, err))
        return false;
      if (!AppendIdentifierValue(scope, literal, identifier, &output, err))
        return false;
    } else {
      output.push_back(input[i]);
    }
  }
  return true;
}

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  CHECK(str.size() >= prefix.size() &&
        str.compare(0, prefix.size(), prefix) == 0);
  return str.substr(prefix.size());
}
