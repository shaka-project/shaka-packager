// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/path_output.h"

#include "build/build_config.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/output_file.h"
#include "tools/gn/string_utils.h"

PathOutput::PathOutput(const SourceDir& current_dir,
                       EscapingMode escaping,
                       bool convert_slashes)
    : current_dir_(current_dir) {
  inverse_current_dir_ = InvertDir(current_dir_);

  options_.mode = escaping;
  options_.convert_slashes = convert_slashes;
  options_.inhibit_quoting = false;

  if (convert_slashes)
    ConvertPathToSystem(&inverse_current_dir_);
}

PathOutput::~PathOutput() {
}

void PathOutput::WriteFile(std::ostream& out, const SourceFile& file) const {
  WritePathStr(out, file.value());
}

void PathOutput::WriteDir(std::ostream& out,
                          const SourceDir& dir,
                          DirSlashEnding slash_ending) const {
  if (dir.value() == "/") {
    // Writing system root is always a slash (this will normally only come up
    // on Posix systems).
    out << "/";
  } else if (dir.value() == "//") {
    // Writing out the source root.
    if (slash_ending == DIR_NO_LAST_SLASH) {
      // The inverse_current_dir_ will contain a [back]slash at the end, so we
      // can't just write it out.
      if (inverse_current_dir_.empty()) {
        out << ".";
      } else {
        out.write(inverse_current_dir_.c_str(),
                  inverse_current_dir_.size() - 1);
      }
    } else {
      if (inverse_current_dir_.empty())
        out << "./";
      else
        out << inverse_current_dir_;
    }
  } else if (slash_ending == DIR_INCLUDE_LAST_SLASH) {
    WritePathStr(out, dir.value());
  } else {
    // DIR_NO_LAST_SLASH mode, just trim the last char.
    WritePathStr(out, base::StringPiece(dir.value().data(),
                                        dir.value().size() - 1));
  }
}

void PathOutput::WriteFile(std::ostream& out, const OutputFile& file) const {
  // Here we assume that the path is already preprocessed.
  EscapeStringToStream(out, file.value(), options_);
}

void PathOutput::WriteSourceRelativeString(
    std::ostream& out,
    const base::StringPiece& str) const {
  // Input begins with two slashes, is relative to source root. Strip off
  // the two slashes when cat-ing it.
  if (options_.mode == ESCAPE_SHELL) {
    // Shell escaping needs an intermediate string since it may end up
    // quoting the whole thing. On Windows, the slashes may already be
    // converted to backslashes in inverse_current_dir_, but we assume that on
    // Windows the escaper won't try to then escape the preconverted
    // backslashes and will just pass them, so this is fine.
    std::string intermediate;
    intermediate.reserve(inverse_current_dir_.size() + str.size());
    intermediate.assign(inverse_current_dir_.c_str(),
                        inverse_current_dir_.size());
    intermediate.append(str.data(), str.size());

    EscapeStringToStream(out,
        base::StringPiece(intermediate.c_str(), intermediate.size()),
        options_);
  } else {
    // Ninja (and none) escaping can avoid the intermediate string and
    // reprocessing of the inverse_current_dir_.
    out << inverse_current_dir_;
    EscapeStringToStream(out, str, options_);
  }
}

void PathOutput::WritePathStr(std::ostream& out,
                              const base::StringPiece& str) const {
  DCHECK(str.size() > 0 && str[0] == '/');

  if (str.size() >= 2 && str[1] == '/') {
    WriteSourceRelativeString(out, str.substr(2));
  } else {
    // Input begins with one slash, don't write the current directory since
    // it's system-absolute.
#if defined(OS_WIN)
    // On Windows, trim the leading slash, since the input for absolute
    // paths will look like "/C:/foo/bar.txt".
    EscapeStringToStream(out, str.substr(1), options_);
#else
    EscapeStringToStream(out, str, options_);
#endif
  }
}
