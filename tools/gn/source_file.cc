// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/source_file.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/source_dir.h"

SourceFile::SourceFile() {
}

SourceFile::SourceFile(const base::StringPiece& p)
    : value_(p.data(), p.size()) {
  DCHECK(!value_.empty());
  DCHECK(value_[0] == '/');
  DCHECK(!EndsWithSlash(value_));
}

SourceFile::~SourceFile() {
}

std::string SourceFile::GetName() const {
  if (is_null())
    return std::string();

  DCHECK(value_.find('/') != std::string::npos);
  size_t last_slash = value_.rfind('/');
  return std::string(&value_[last_slash + 1],
                     value_.size() - last_slash - 1);
}

SourceDir SourceFile::GetDir() const {
  if (is_null())
    return SourceDir();

  DCHECK(value_.find('/') != std::string::npos);
  size_t last_slash = value_.rfind('/');
  return SourceDir(base::StringPiece(&value_[0], last_slash + 1));
}

base::FilePath SourceFile::Resolve(const base::FilePath& source_root) const {
  if (is_null())
    return base::FilePath();

  std::string converted;
#if defined(OS_WIN)
  if (is_system_absolute()) {
    converted.assign(&value_[1], value_.size() - 1);
    DCHECK(converted.size() > 2 && converted[1] == ':')
        << "Expecting Windows absolute file path with a drive letter: "
        << value_;
    return base::FilePath(UTF8ToFilePath(converted));
  }

  converted.assign(&value_[2], value_.size() - 2);
  ConvertPathToSystem(&converted);
  return source_root.Append(UTF8ToFilePath(converted));
#else
  if (is_system_absolute())
    return base::FilePath(value_);
  converted.assign(&value_[2], value_.size() - 2);
  return source_root.Append(converted);
#endif
}
