// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/commands.h"

#include "tools/gn/item.h"
#include "tools/gn/item_node.h"
#include "tools/gn/label.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace commands {

CommandInfo::CommandInfo()
    : help_short(NULL),
      help(NULL),
      runner(NULL) {
}

CommandInfo::CommandInfo(const char* in_help_short,
                         const char* in_help,
                         CommandRunner in_runner)
    : help_short(in_help_short),
      help(in_help),
      runner(in_runner) {
}

const CommandInfoMap& GetCommands() {
  static CommandInfoMap info_map;
  if (info_map.empty()) {
    #define INSERT_COMMAND(cmd) \
        info_map[k##cmd] = CommandInfo(k##cmd##_HelpShort, \
                                       k##cmd##_Help, \
                                       &Run##cmd);

    INSERT_COMMAND(Desc)
    INSERT_COMMAND(Gen)
    INSERT_COMMAND(Help)

    #undef INSERT_COMMAND
  }
  return info_map;
}

const Target* GetTargetForDesc(const std::vector<std::string>& args) {
  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  if (!setup->DoSetup())
    return NULL;

  // FIXME(brettw): set the output dir to be a sandbox one to avoid polluting
  // the real output dir with files written by the build scripts.

  // Do the actual load. This will also write out the target ninja files.
  if (!setup->Run())
    return NULL;

  // Need to resolve the label after we know the default toolchain.
  // TODO(brettw) find the current directory and resolve the input label
  // relative to that.
  Label default_toolchain = setup->build_settings().toolchain_manager()
      .GetDefaultToolchainUnlocked();
  Value arg_value(NULL, args[0]);
  Err err;
  Label label = Label::Resolve(SourceDir(), default_toolchain, arg_value, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return NULL;
  }

  ItemNode* node;
  {
    base::AutoLock lock(setup->build_settings().item_tree().lock());
    node = setup->build_settings().item_tree().GetExistingNodeLocked(label);
  }
  if (!node) {
    Err(Location(), "",
        "I don't know about this \"" + label.GetUserVisibleName(false) +
        "\"").PrintToStdout();
    return NULL;
  }

  const Target* target = node->item()->AsTarget();
  if (!target) {
    Err(Location(), "Not a target.",
        "The \"" + label.GetUserVisibleName(false) + "\" thing\n"
        "is not a target. Somebody should probably implement this command for "
        "other\nitem types.");
    return NULL;
  }

  return target;
}

}  // namespace commands
