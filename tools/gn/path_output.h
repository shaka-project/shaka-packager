// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_PATH_OUTPUT_H_
#define TOOLS_GN_PATH_OUTPUT_H_

#include <iosfwd>
#include <string>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "tools/gn/escape.h"
#include "tools/gn/source_dir.h"

class OutputFile;
class SourceFile;

// Writes file names to streams assuming a certain input directory and
// escaping rules. This gives us a central place for managing this state.
class PathOutput {
 public:
  // Controls whether writing directory names include the trailing slash.
  // Often we don't want the trailing slash when writing out to a command line,
  // especially on Windows where it's a backslash and might be interpreted as
  // escaping the thing following it.
  enum DirSlashEnding {
    DIR_INCLUDE_LAST_SLASH,
    DIR_NO_LAST_SLASH,
  };

  PathOutput(const SourceDir& current_dir,
             EscapingMode escaping,
             bool convert_slashes);
  ~PathOutput();

  // Read-only since inverse_current_dir_ is computed depending on this.
  EscapingMode escaping_mode() const { return options_.mode; }

  // When true, converts slashes to the system-type path separators (on
  // Windows, this is a backslash, this is a NOP otherwise).
  //
  // Read-only since inverse_current_dir_ is computed depending on this.
  bool convert_slashes_to_system() const { return options_.convert_slashes; }

  // When the output escaping is ESCAPE_SHELL, the escaper will normally put
  // quotes around suspect things. If this value is set to true, we'll disable
  // the quoting feature. This means that in ESCAPE_SHELL mode, strings with
  // spaces in them qon't be quoted. This mode is for when quoting is done at
  // some higher-level. Defaults to false.
  bool inhibit_quoting() const { return options_.inhibit_quoting; }
  void set_inhibit_quoting(bool iq) { options_.inhibit_quoting = iq; }

  void WriteFile(std::ostream& out, const SourceFile& file) const;
  void WriteFile(std::ostream& out, const OutputFile& file) const;
  void WriteDir(std::ostream& out,
                const SourceDir& dir,
                DirSlashEnding slash_ending) const;

  // Backend for WriteFile and WriteDir. This appends the given file or
  // directory string to the file.
  void WritePathStr(std::ostream& out, const base::StringPiece& str) const;

 private:
  // Takes the given string and writes it out, appending to the inverse
  // current dir. This assumes leading slashes have been trimmed.
  void WriteSourceRelativeString(std::ostream& out,
                                 const base::StringPiece& str) const;

  SourceDir current_dir_;

  // Uses system slashes if convert_slashes_to_system_.
  std::string inverse_current_dir_;

  // Since the inverse_current_dir_ depends on some of these, we don't expose
  // this directly to modification.
  EscapeOptions options_;
};

#endif  // TOOLS_GN_PATH_OUTPUT_H_
