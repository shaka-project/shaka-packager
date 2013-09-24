// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_conversion.h"
#include "tools/gn/input_file.h"
#include "tools/gn/scheduler.h"

// TODO(brettw) consider removing this. I originally wrote it for making the
// WebKit bindings but misundersood what was required, and didn't need to
// use this. This seems to have a high potential for misuse.

namespace functions {

const char kReadFile[] = "read_file";
const char kReadFile_Help[] =
    "read_file: Read a file into a variable.\n"
    "\n"
    "  read_file(filename, how_to_read)\n"
    "\n"
    "  Whitespace will be trimmed from the end of the file. Throws an error\n"
    "  if the file can not be opened.\n"
    "\n"
    "Arguments:\n"
    "\n"
    "  filename\n"
    "      Filename to read, relative to the build file.\n"
    "\n"
    "  input_conversion\n"
    "      Controls how the file is read and parsed.\n"
    "      See \"gn help input_conversion\".\n"
    "\n"
    "Example:\n"
    "  lines = read_file(\"foo.txt\", \"list lines\")\n";

Value RunReadFile(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  Err* err) {
  if (args.size() != 2) {
    *err = Err(function->function(), "Wrong number of args to read_file",
               "I expected two arguments.");
    return Value();
  }
  if (!args[0].VerifyTypeIs(Value::STRING, err))
    return Value();

  // Compute the file name.
  const SourceDir& cur_dir = SourceDirForFunctionCall(function);
  SourceFile source_file = cur_dir.ResolveRelativeFile(args[0].string_value());
  base::FilePath file_path =
      scope->settings()->build_settings()->GetFullPath(source_file);

  // Ensure that everything is recomputed if the read file changes.
  g_scheduler->AddGenDependency(file_path);

  // Read contents.
  std::string file_contents;
  if (!file_util::ReadFileToString(file_path, &file_contents)) {
    *err = Err(args[0], "Could not read file.",
               "I resolved this to \"" + FilePathToUTF8(file_path) + "\".");
    return Value();
  }

  return ConvertInputToValue(file_contents, function, args[1], err);
}

}  // namespace functions
