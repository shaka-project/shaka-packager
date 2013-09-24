// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SETUP_H_
#define TOOLS_GN_SETUP_H_

#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/token.h"
#include "tools/gn/toolchain.h"

class CommandLine;
class InputFile;
class ParseNode;

extern const char kDotfile_Help[];

// Helper class to setup the build settings and environment for the various
// commands to run.
class Setup {
 public:
  Setup();
  ~Setup();

  // Configures the build for the current command line. On success returns
  // true. On failure, prints the error and returns false.
  bool DoSetup();

  // Runs the load, returning true on success. On failure, prints the error
  // and returns false.
  bool Run();

  BuildSettings& build_settings() { return build_settings_; }
  Scheduler& scheduler() { return scheduler_; }

 private:
  // Fills the root directory into the settings. Returns true on success.
  bool FillSourceDir(const CommandLine& cmdline);

  // Run config file.
  bool RunConfigFile();

  bool FillOtherConfig(const CommandLine& cmdline);

  BuildSettings build_settings_;
  Scheduler scheduler_;

  // State for invoking the dotfile.
  // TODO(brettw) this seems a bit excessive, maybe we can get this down
  // somehow?
  base::FilePath dotfile_name_;
  scoped_ptr<InputFile> dotfile_input_file_;
  std::vector<Token> dotfile_tokens_;
  scoped_ptr<ParseNode> dotfile_root_;
  BuildSettings dotfile_build_settings_;
  Toolchain dotfile_toolchain_;
  Settings dotfile_settings_;
  Scope dotfile_scope_;

  DISALLOW_COPY_AND_ASSIGN(Setup);
};

#endif  // TOOLS_GN_SETUP_H_
