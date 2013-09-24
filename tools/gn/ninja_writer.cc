// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_writer.h"

#include "tools/gn/location.h"
#include "tools/gn/ninja_build_writer.h"
#include "tools/gn/ninja_toolchain_writer.h"


NinjaWriter::NinjaWriter(const BuildSettings* build_settings)
    : build_settings_(build_settings) {
}

NinjaWriter::~NinjaWriter() {
}

// static
bool NinjaWriter::RunAndWriteFiles(const BuildSettings* build_settings) {
  NinjaWriter writer(build_settings);
  return writer.WriteRootBuildfiles();
}

bool NinjaWriter::WriteRootBuildfiles() {
  // Categorize all targets by toolchain.
  typedef std::map<Label, std::vector<const Target*> > CategorizedMap;
  CategorizedMap categorized;

  std::vector<const Target*> all_targets;
  build_settings_->target_manager().GetAllTargets(&all_targets);
  if (all_targets.empty()) {
    Err(Location(), "No targets.",
        "I could not find any targets to write, so I'm doing nothing.")
        .PrintToStdout();
    return false;
  }
  for (size_t i = 0; i < all_targets.size(); i++) {
    categorized[all_targets[i]->label().GetToolchainLabel()].push_back(
        all_targets[i]);
  }

  Label default_label =
      build_settings_->toolchain_manager().GetDefaultToolchainUnlocked();

  // Write out the toolchain buildfiles, and also accumulate the set of
  // all settings and find the list of targets in the default toolchain.
  std::vector<const Settings*> all_settings;
  const std::vector<const Target*>* default_targets = NULL;
  for (CategorizedMap::const_iterator i = categorized.begin();
       i != categorized.end(); ++i) {
    const Settings* settings;
    {
      base::AutoLock lock(build_settings_->item_tree().lock());
      Err ignored;
      settings =
          build_settings_->toolchain_manager().GetSettingsForToolchainLocked(
              LocationRange(), i->first, &ignored);
    }
    if (i->first == default_label)
      default_targets = &i->second;
    all_settings.push_back(settings);
    if (!NinjaToolchainWriter::RunAndWriteFile(settings, i->second)) {
      Err(Location(),
          "Couldn't open toolchain buildfile(s) for writing").PrintToStdout();
      return false;
    }
  }

  // Write the root buildfile.
  if (!NinjaBuildWriter::RunAndWriteFile(build_settings_, all_settings,
                                         *default_targets)) {
    Err(Location(),
        "Couldn't open toolchain buildfile(s) for writing").PrintToStdout();
    return false;
  }
  return true;
}
