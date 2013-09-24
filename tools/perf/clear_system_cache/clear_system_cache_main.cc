// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A utility to clear the operating system's cache for a file or directory.
//
// USAGE: clear_system_cache [--recurse] <files or directories>

#include <stdio.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/test/test_file_util.h"

void ClearCacheForFile(const base::FilePath& path) {
  VLOG(1) << "Clearing " << path.MaybeAsASCII();
  file_util::EvictFileFromSystemCache(path);
}

int main(int argc, const char* argv[]) {
  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  bool should_recurse = parsed_command_line.HasSwitch("recurse");
  const CommandLine::StringVector& args = parsed_command_line.GetArgs();

  if (args.size() < 1) {
    printf("USAGE: %s [--recurse] <files or directories>\n", argv[0]);
    return 1;
  }

  for (size_t i = 0; i < args.size(); ++i) {
    base::FilePath path(args[i]);
    if (!base::PathExists(path)) {
      LOG(ERROR) << "Couldn't find " << path.MaybeAsASCII();
      return 1;
    }

    if (base::DirectoryExists(path)) {
      base::FileEnumerator enumerator(path, should_recurse,
                                      base::FileEnumerator::FILES);
      for (base::FilePath next = enumerator.Next(); !next.empty();
           next = enumerator.Next()) {
        ClearCacheForFile(next);
      }
    } else {
      ClearCacheForFile(path);
    }
  }

  return 0;
}
