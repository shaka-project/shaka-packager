// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/location.h"

namespace {

std::vector<std::string> GetArgs(const CommandLine& cmdline) {
  CommandLine::StringVector in_args = cmdline.GetArgs();
#if defined(OS_WIN)
  std::vector<std::string> out_args;
  for (size_t i = 0; i < in_args.size(); i++)
    out_args.push_back(base::WideToUTF8(in_args[i]));
  return out_args;
#else
  return in_args;
#endif
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);

  const CommandLine& cmdline = *CommandLine::ForCurrentProcess();
  std::vector<std::string> args = GetArgs(cmdline);

  std::string command;
  if (cmdline.HasSwitch("help")) {
    // Make "--help" default to help command.
    command = commands::kHelp;
  } else if (args.empty()) {
    command = commands::kGen;
  } else {
    command = args[0];
    args.erase(args.begin());
  }

  const commands::CommandInfoMap& command_map = commands::GetCommands();
  commands::CommandInfoMap::const_iterator found_command =
      command_map.find(command);

  int retval;
  if (found_command != command_map.end()) {
    retval = found_command->second.runner(args);
  } else {
    Err(Location(),
        "Command \"" + command + "\" unknown.").PrintToStdout();
    commands::RunHelp(std::vector<std::string>());
    retval = 1;
  }

  exit(retval);  // Don't free memory, it can be really slow!
  return retval;
}
