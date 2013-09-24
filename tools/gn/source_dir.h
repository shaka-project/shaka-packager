// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SOURCE_DIR_H_
#define TOOLS_GN_SOURCE_DIR_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"

class SourceFile;

// Represents a directory within the source tree. Source dirs begin and end in
// slashes.
//
// If there is one slash at the beginning, it will mean a system-absolute file
// path. On Windows, absolute system paths will be of the form "/C:/foo/bar".
//
// Two slashes at the beginning indicate a path relative to the source root.
class SourceDir {
 public:
  SourceDir();
  explicit SourceDir(const base::StringPiece& p);
  ~SourceDir();

  // Resolves a file or dir name relative to this source directory. Will return
  // an empty SourceDir/File on error. Empty input is always an error (it's
  // possible we should say ResolveRelativeDir vs. an empty string should be
  // the source dir, but we require "." instead).
  SourceFile ResolveRelativeFile(const base::StringPiece& p) const;
  SourceDir ResolveRelativeDir(const base::StringPiece& p) const;

  // Resolves this source file relative to some given source root. Returns
  // an empty file path on error.
  base::FilePath Resolve(const base::FilePath& source_root) const;

  bool is_null() const { return value_.empty(); }
  const std::string& value() const { return value_; }

  // Returns true if this path starts with a "//" which indicates a path
  // from the source root.
  bool is_source_absolute() const {
    return value_.size() >= 2 && value_[0] == '/' && value_[1] == '/';
  }

  // Returns true if this path starts with a single slash which indicates a
  // system-absolute path.
  bool is_system_absolute() const {
    return !is_source_absolute();
  }

  // Returns a source-absolute path starting with only one slash at the
  // beginning (normally source-absolute paths start with two slashes to mark
  // them as such). This is normally used when concatenating directories
  // together.
  //
  // This function asserts that the directory is actually source-absolute. The
  // return value points into our buffer.
  base::StringPiece SourceAbsoluteWithOneSlash() const {
    CHECK(is_source_absolute());
    return base::StringPiece(&value_[1], value_.size() - 1);
  }

  void SwapInValue(std::string* v);

  bool operator==(const SourceDir& other) const {
    return value_ == other.value_;
  }
  bool operator!=(const SourceDir& other) const {
    return !operator==(other);
  }
  bool operator<(const SourceDir& other) const {
    return value_ < other.value_;
  }

 private:
  friend class SourceFile;
  std::string value_;

  // Copy & assign supported.
};

namespace BASE_HASH_NAMESPACE {

#if defined(COMPILER_GCC)
template<> struct hash<SourceDir> {
  std::size_t operator()(const SourceDir& v) const {
    hash<std::string> h;
    return h(v.value());
  }
};
#elif defined(COMPILER_MSVC)
inline size_t hash_value(const SourceDir& v) {
  return hash_value(v.value());
}
#endif  // COMPILER...

}  // namespace BASE_HASH_NAMESPACE

#endif  // TOOLS_GN_SOURCE_DIR_H_
