// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/string_escape.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace base {

namespace {

// Try to escape |c| as a "SingleEscapeCharacter" (\n, etc).  If successful,
// returns true and appends the escape sequence to |dst|.  This isn't required
// by the spec, but it's more readable by humans than the \uXXXX alternatives.
template<typename CHAR>
static bool JsonSingleEscapeChar(const CHAR c, std::string* dst) {
  // WARNING: if you add a new case here, you need to update the reader as well.
  // Note: \v is in the reader, but not here since the JSON spec doesn't
  // allow it.
  switch (c) {
    case '\b':
      dst->append("\\b");
      break;
    case '\f':
      dst->append("\\f");
      break;
    case '\n':
      dst->append("\\n");
      break;
    case '\r':
      dst->append("\\r");
      break;
    case '\t':
      dst->append("\\t");
      break;
    case '\\':
      dst->append("\\\\");
      break;
    case '"':
      dst->append("\\\"");
      break;
    default:
      return false;
  }
  return true;
}

template <class STR>
void JsonDoubleQuoteT(const STR& str,
                      bool put_in_quotes,
                      std::string* dst) {
  if (put_in_quotes)
    dst->push_back('"');

  for (typename STR::const_iterator it = str.begin(); it != str.end(); ++it) {
    typename ToUnsigned<typename STR::value_type>::Unsigned c = *it;
    if (!JsonSingleEscapeChar(c, dst)) {
      if (c < 32 || c > 126 || c == '<' || c == '>') {
        // 1. Escaping <, > to prevent script execution.
        // 2. Technically, we could also pass through c > 126 as UTF8, but this
        //    is also optional.  It would also be a pain to implement here.
        unsigned int as_uint = static_cast<unsigned int>(c);
        base::StringAppendF(dst, "\\u%04X", as_uint);
      } else {
        unsigned char ascii = static_cast<unsigned char>(*it);
        dst->push_back(ascii);
      }
    }
  }

  if (put_in_quotes)
    dst->push_back('"');
}

}  // namespace

void JsonDoubleQuote(const std::string& str,
                     bool put_in_quotes,
                     std::string* dst) {
  JsonDoubleQuoteT(str, put_in_quotes, dst);
}

std::string GetDoubleQuotedJson(const std::string& str) {
  std::string dst;
  JsonDoubleQuote(str, true, &dst);
  return dst;
}

void JsonDoubleQuote(const string16& str,
                     bool put_in_quotes,
                     std::string* dst) {
  JsonDoubleQuoteT(str, put_in_quotes, dst);
}

std::string GetDoubleQuotedJson(const string16& str) {
  std::string dst;
  JsonDoubleQuote(str, true, &dst);
  return dst;
}

}  // namespace base
