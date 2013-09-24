// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/value_extractors.h"

#include "tools/gn/err.h"
#include "tools/gn/label.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"

namespace {

// This extractor rejects files with system-absolute file paths. If we need
// that in the future, we'll have to add some flag to control this.
struct RelativeFileConverter {
  RelativeFileConverter(const SourceDir& current_dir_in)
      : current_dir(current_dir_in) {}
  bool operator()(const Value& v, SourceFile* out, Err* err) const {
    if (!v.VerifyTypeIs(Value::STRING, err))
      return false;
    *out = current_dir.ResolveRelativeFile(v.string_value());
    if (out->is_system_absolute()) {
      *err = Err(v, "System-absolute file path.",
          "You can't list a system-absolute file path here. Please include "
          "only files in\nthe source tree. Maybe you meant to begin with two "
          "slashes to indicate an\nabsolute path in the source tree?");
      return false;
    }
    return true;
  }
  const SourceDir& current_dir;
};

struct RelativeDirConverter {
  RelativeDirConverter(const SourceDir& current_dir_in)
      : current_dir(current_dir_in) {}
  bool operator()(const Value& v, SourceDir* out, Err* err) const {
    if (!v.VerifyTypeIs(Value::STRING, err))
      return false;
    *out = current_dir.ResolveRelativeDir(v.string_value());
    return true;
  }
  const SourceDir& current_dir;
};

struct LabelResolver {
  LabelResolver(const SourceDir& current_dir_in,
                const Label& current_toolchain_in)
      : current_dir(current_dir_in),
        current_toolchain(current_toolchain_in) {}
  bool operator()(const Value& v, Label* out, Err* err) const {
    if (!v.VerifyTypeIs(Value::STRING, err))
      return false;
    *out = Label::Resolve(current_dir, current_toolchain, v, err);
    return !err->has_error();
  }
  const SourceDir& current_dir;
  const Label& current_toolchain;
};

}  // namespace

bool ExtractListOfStringValues(const Value& value,
                               std::vector<std::string>* dest,
                               Err* err) {
  if (!value.VerifyTypeIs(Value::LIST, err))
    return false;
  const std::vector<Value>& input_list = value.list_value();
  dest->reserve(input_list.size());
  for (size_t i = 0; i < input_list.size(); i++) {
    if (!input_list[i].VerifyTypeIs(Value::STRING, err))
      return false;
    dest->push_back(input_list[i].string_value());
  }
  return true;
}

bool ExtractListOfRelativeFiles(const Value& value,
                                const SourceDir& current_dir,
                                std::vector<SourceFile>* files,
                                Err* err) {
  return ListValueExtractor(value, files, err,
                            RelativeFileConverter(current_dir));
}

bool ExtractListOfRelativeDirs(const Value& value,
                               const SourceDir& current_dir,
                               std::vector<SourceDir>* dest,
                               Err* err) {
  return ListValueExtractor(value, dest, err,
                            RelativeDirConverter(current_dir));
}

bool ExtractListOfLabels(const Value& value,
                         const SourceDir& current_dir,
                         const Label& current_toolchain,
                         std::vector<Label>* dest,
                         Err* err) {
  return ListValueExtractor(value, dest, err,
                            LabelResolver(current_dir, current_toolchain));
}
