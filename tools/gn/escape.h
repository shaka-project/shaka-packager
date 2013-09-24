// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ESCAPE_H_
#define TOOLS_GN_ESCAPE_H_

#include <iosfwd>

#include "base/strings/string_piece.h"

// TODO(brettw) we may need to make this a bitfield. If we want to write a
// shell command in a ninja file, we need the shell characters to be escaped,
// and THEN the ninja characters. Or maybe we require the caller to do two
// passes.
enum EscapingMode {
  ESCAPE_NONE,   // No escaping.
  ESCAPE_NINJA,  // Ninja string escaping.
  ESCAPE_SHELL,  // Shell string escaping.
};

struct EscapeOptions {
  EscapeOptions()
      : mode(ESCAPE_NONE),
        convert_slashes(false),
        inhibit_quoting(false) {
  }

  EscapingMode mode;

  // When set, converts forward-slashes to system-specific path separators.
  bool convert_slashes;

  // When the escaping mode is ESCAPE_SHELL, the escaper will normally put
  // quotes around things with spaces. If this value is set to true, we'll
  // disable the quoting feature and just add the spaces.
  //
  // This mode is for when quoting is done at some higher-level. Defaults to
  // false.
  bool inhibit_quoting;
};

// Escapes the given input, returnining the result.
std::string EscapeString(const base::StringPiece& str,
                         const EscapeOptions& options);

// Same as EscapeString but writes the results to the given stream, saving a
// copy.
void EscapeStringToStream(std::ostream& out,
                          const base::StringPiece& str,
                          const EscapeOptions& options);

#endif  // TOOLS_GN_ESCAPE_H_
