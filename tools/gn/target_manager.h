// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_MANAGER_H_
#define TOOLS_GN_TARGET_MANAGER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/synchronization/lock.h"
#include "tools/gn/target.h"

class BuildSettings;
class Err;
class ItemTree;
class LocationRange;
class ToolchainManager;
class Value;

// Manages all the targets in the system. This integrates with the ItemTree
// to manage the target-specific rules and creation.
//
// This class is threadsafe.
class TargetManager {
 public:
  explicit TargetManager(const BuildSettings* settings);
  ~TargetManager();

  // Gets a reference to a named target. The given target name is created if
  // it doesn't exist.
  //
  // The label should be fully specified in that it should include an
  // explicit toolchain.
  //
  // |specified_from_here| should indicate the dependency or the target
  // generator causing this access for error message generation.
  //
  // |dep_from| should be set when a target is getting a dep that it depends
  // on. |dep_from| indicates the target that specified the dependency. It
  // will be used to track outstanding dependencies so we can know when the
  // target and all of its dependencies are complete. It should be null when
  // getting a target for other reasons.
  //
  // On failure, |err| will be set.
  //
  // The returned pointer must not be dereferenced until it's generated, since
  // it could be being generated on another thread.
  Target* GetTarget(const Label& label,
                    const LocationRange& specified_from_here,
                    Target* dep_from,
                    Err* err);

  // Called by a target when it has been loaded from the .gin file. Its
  // dependencies may or may not be resolved yet.
  bool TargetGenerationComplete(const Label& label, Err* err);

  // Returns a list of all targets.
  void GetAllTargets(std::vector<const Target*>* all_targets) const;

 private:
  const BuildSettings* build_settings_;

  DISALLOW_COPY_AND_ASSIGN(TargetManager);
};

#endif  // TOOLS_GN_TARGET_MANAGER_H
