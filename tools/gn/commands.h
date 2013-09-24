// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_COMMANDS_H_
#define TOOLS_GN_COMMANDS_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"

class Target;

// Each "Run" command returns the value we should return from main().

namespace commands {

typedef int (*CommandRunner)(const std::vector<std::string>&);

extern const char kDesc[];
extern const char kDesc_HelpShort[];
extern const char kDesc_Help[];
int RunDesc(const std::vector<std::string>& args);

extern const char kGen[];
extern const char kGen_HelpShort[];
extern const char kGen_Help[];
int RunGen(const std::vector<std::string>& args);

extern const char kHelp[];
extern const char kHelp_HelpShort[];
extern const char kHelp_Help[];
int RunHelp(const std::vector<std::string>& args);

// -----------------------------------------------------------------------------

struct CommandInfo {
  CommandInfo();
  CommandInfo(const char* in_help_short,
              const char* in_help,
              CommandRunner in_runner);

  const char* help_short;
  const char* help;
  CommandRunner runner;
};

typedef std::map<base::StringPiece, CommandInfo> CommandInfoMap;

const CommandInfoMap& GetCommands();

// Helper functions for some commands ------------------------------------------

// Runs a build for the given command line, returning the target identified by
// the first non-switch command line parameter.
//
// Note that a lot of memory is leaked to avoid proper teardown under the
// assumption that you will only run this once and exit.
//
// On failure, prints an error message and returns NULL.
const Target* GetTargetForDesc(const std::vector<std::string>& args);

}  // namespace commands

#endif  // TOOLS_GN_COMMANDS_H_
