// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Makes a given program ("Google Chrome" by default) the default handler for
// some URL protocol ("http" by default) on Windows 8. These defaults can be
// overridden via the --program and --protocol command line switches.

#include <windows.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "ui/base/win/atl_module.h"
#include "win8/test/open_with_dialog_controller.h"

namespace {

const char kSwitchProgram[] = "program";
const char kSwitchProtocol[] = "protocol";
const wchar_t kDefaultProgram[] = L"Google Chrome";
const wchar_t kDefaultProtocol[] = L"http";

}  // namespace

extern "C"
int wmain(int argc, wchar_t* argv[]) {
  // Initialize the commandline singleton from the environment.
  CommandLine::Init(0, NULL);
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  settings.dcheck_state =
      logging::ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS;
  logging::InitLogging(settings);
  logging::SetMinLogLevel(logging::LOG_VERBOSE);

  ui::win::CreateATLModuleIfNeeded();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  string16 protocol(command_line->GetSwitchValueNative(kSwitchProtocol));
  if (protocol.empty())
    protocol = kDefaultProtocol;

  string16 program(command_line->GetSwitchValueNative(kSwitchProgram));
  if (program.empty())
    program = kDefaultProgram;

  std::vector<string16> choices;
  HRESULT result = S_OK;
  win8::OpenWithDialogController controller;
  result = controller.RunSynchronously(NULL, protocol, program, &choices);

  if (SUCCEEDED(result)) {
    printf("success\n");
  } else if (!choices.empty()) {
    printf("failed to set program. possible choices: %ls\n",
           JoinString(choices, L", ").c_str());
  } else {
    printf("failed with HRESULT: 0x08X\n", result);
  }

  return FAILED(result);
}
