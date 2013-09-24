// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/build_settings.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/toolchain_manager.h"

namespace functions {

const char kSetDefaultToolchain[] = "set_default_toolchain";
const char kSetDefaultToolchain_Help[] =
    "set_default_toolchain: Sets the default toolchain name.\n"
    "\n"
    "  set_default_toolchain(toolchain_label)\n"
    "\n"
    "  The given label should identify a toolchain definition (see\n"
    "  \"help toolchain\"). This toolchain will be used for all targets\n"
    "  unless otherwise specified.\n"
    "\n"
    "  This function is only valid to call during the processing of the build\n"
    "  configuration file. Since the build configuration file is processed\n"
    "  separately for each toolchain, this function will be a no-op when\n"
    "  called under any non-default toolchains.\n"
    "\n"
    "  For example, the default toolchain should be appropriate for the\n"
    "  current environment. If the current environment is 32-bit and \n"
    "  somebody references a target with a 64-bit toolchain, we wouldn't\n"
    "  want processing of the build config file for the 64-bit toolchain to\n"
    "  reset the default toolchain to 64-bit, we want to keep it 32-bits.\n"
    "\n"
    "Argument:\n"
    "\n"
    "  toolchain_label\n"
    "      Toolchain name.\n"
    "\n"
    "Example:\n"
    "\n"
    "  set_default_toolchain(\"//build/config/win:vs32\")";

Value RunSetDefaultToolchain(Scope* scope,
                             const FunctionCallNode* function,
                             const std::vector<Value>& args,
                             Err* err) {
  if (!scope->IsProcessingBuildConfig()) {
    *err = Err(function->function(), "Must be called from build config.",
        "set_default_toolchain can only be called from the build configuration "
        "file.");
    return Value();
  }

  // Ignore non-default-build-config invocations.
  if (!scope->IsProcessingDefaultBuildConfig())
    return Value();

  const SourceDir& current_dir = SourceDirForFunctionCall(function);
  const Label& default_toolchain = ToolchainLabelForScope(scope);

  if (!EnsureSingleStringArg(function, args, err))
    return Value();
  Label toolchain_label(
      Label::Resolve(current_dir, default_toolchain, args[0], err));
  if (toolchain_label.is_null())
    return Value();

  ToolchainManager& mgr =
      scope->settings()->build_settings()->toolchain_manager();
  mgr.SetDefaultToolchainUnlocked(toolchain_label, function->GetRange(), err);
  return Value();
}

}  // namespace functions
