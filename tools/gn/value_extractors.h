// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VALUE_EXTRACTORS_H_
#define TOOLS_GN_VALUE_EXTRACTORS_H_

#include <string>
#include <vector>

#include "tools/gn/value.h"

class Err;
class Label;
class SourceDir;
class SourceFile;

// Sets the error and returns false on failure.
template<typename T, class Converter>
bool ListValueExtractor(const Value& value, std::vector<T>* dest,
                        Err* err,
                        const Converter& converter) {
  if (!value.VerifyTypeIs(Value::LIST, err))
    return false;
  const std::vector<Value>& input_list = value.list_value();
  dest->resize(input_list.size());
  for (size_t i = 0; i < input_list.size(); i++) {
    if (!converter(input_list[i], &(*dest)[i], err))
      return false;
  }
  return true;
}

// On failure, returns false and sets the error.
bool ExtractListOfStringValues(const Value& value,
                               std::vector<std::string>* dest,
                               Err* err);

// Looks for a list of source files relative to a given current dir.
bool ExtractListOfRelativeFiles(const Value& value,
                                const SourceDir& current_dir,
                                std::vector<SourceFile>* files,
                                Err* err);

// Looks for a list of source directories relative to a given current dir.
bool ExtractListOfRelativeDirs(const Value& value,
                               const SourceDir& current_dir,
                               std::vector<SourceDir>* dest,
                               Err* err);

bool ExtractListOfLabels(const Value& value,
                         const SourceDir& current_dir,
                         const Label& current_toolchain,
                         std::vector<Label>* dest,
                         Err* err);

#endif  // TOOLS_GN_VALUE_EXTRACTORS_H_
