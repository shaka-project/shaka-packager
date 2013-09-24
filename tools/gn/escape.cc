// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/escape.h"

#include "base/containers/stack_container.h"

namespace {

template<typename DestString>
void EscapeStringToString(const base::StringPiece& str,
                          const EscapeOptions& options,
                          DestString* dest) {
  bool used_quotes = false;

  for (size_t i = 0; i < str.size(); i++) {
    if (str[i] == '$' && options.mode == ESCAPE_NINJA) {
      // Escape dollars signs since ninja treats these specially.
      dest->push_back('$');
      dest->push_back('$');
    } else if (str[i] == '"' && options.mode == ESCAPE_SHELL) {
      // Escape quotes with backslashes for the command-line (Ninja doesn't
      // care).
      dest->push_back('\\');
      dest->push_back('"');
    } else if (str[i] == ' ') {
      if (options.mode == ESCAPE_NINJA) {
        // For ninja just escape spaces with $.
        dest->push_back('$');
      } else if (options.mode == ESCAPE_SHELL && !options.inhibit_quoting) {
        // For the shell, quote the whole string.
        if (!used_quotes) {
          used_quotes = true;
          dest->insert(dest->begin(), '"');
        }
      }
      dest->push_back(' ');
#if defined(OS_WIN)
    } else if (str[i] == '/' && options.convert_slashes) {
      // Convert slashes on Windows if requested.
      dest->push_back('\\');
#else
    } else if (str[i] == '\\' && options.mode == ESCAPE_SHELL) {
      // For non-Windows shell, escape backslashes.
      dest->push_back('\\');
      dest->push_back('\\');
#endif
    } else {
      dest->push_back(str[i]);
    }
  }

  if (used_quotes)
    dest->push_back('"');
}

}  // namespace

std::string EscapeString(const base::StringPiece& str,
                         const EscapeOptions& options) {
  std::string result;
  result.reserve(str.size() + 4);  // Guess we'll add a couple of extra chars.
  EscapeStringToString(str, options, &result);
  return result;
}

void EscapeStringToStream(std::ostream& out,
                          const base::StringPiece& str,
                          const EscapeOptions& options) {
  // Escape to a stack buffer and then write out to the stream.
  base::StackVector<char, 256> result;
  result->reserve(str.size() + 4);  // Guess we'll add a couple of extra chars.
  EscapeStringToString(str, options, &result.container());
  if (!result->empty())
    out.write(result->data(), result->size());
}
