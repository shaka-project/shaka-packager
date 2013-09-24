// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TOOLCHAIN_MANAGER_H_
#define TOOLS_GN_TOOLCHAIN_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "tools/gn/label.h"
#include "tools/gn/location.h"
#include "tools/gn/source_file.h"
#include "tools/gn/toolchain.h"

class Err;
class BuildSettings;
class ParseNode;
class Settings;

// The toolchain manager manages the mapping of toolchain names to the
// settings and toolchain object. It also loads build files in the context of a
// toolchain context of a toolchain, and manages running the build config
// script when necessary.
//
// This class uses the lock from the item tree to manage threadsafety. The
// functions requiring this lock to be held are named "Locked" to make this
// more clear. The "Unlocked" versions will acquire the lock themselves so will
// break if you call it while locked. (The rationale behind which is which is
// just based on the needs of the callers, so it can be changed.) There are two
// reasons for this:
//
// The first is that when resolving a target, we do a bunch of script
// stuff (slow) and then lookup the target, config, and toolchain dependencies
// based on that. The options are to do a lock around each dependency lookup
// or do a lock around the entire operation. Given that there's not a huge
// amount of work, the "big lock" approach is likely a bit better since it
// avoids lots of locking overhead.
//
// The second reason is that if we had a separate lock here, we would need to
// lock around creating a new toolchain. But creating a new toolchain involves
// adding it to the item tree, and this needs to be done atomically to prevent
// other threads from seeing a partially initialized toolchain. This sets up
// having deadlock do to acquiring multiple locks, or recursive locking
// problems.
class ToolchainManager {
 public:
  ToolchainManager(const BuildSettings* build_settings);
  ~ToolchainManager();

  // At the very beginning of processing, this begins loading build files.
  // This will scheduler loadin the default build config and the given build
  // file in that context, going out from there.
  //
  // This returns immediately, you need to run the Scheduler to actually
  // process anything. It's assumed this function is called on the main thread
  // before doing anything, so it does not need locking.
  void StartLoadingUnlocked(const SourceFile& build_file_name);

  // Returns the settings object for a given toolchain. This does not
  // schedule loading the given toolchain if it's not loaded yet: you actually
  // need to invoke a target with that toolchain to get that.
  //
  // On error, returns NULL and sets the error.
  const Settings* GetSettingsForToolchainLocked(const LocationRange& from_here,
                                                const Label& toolchain_name,
                                                Err* err);

  // Returns the toolchain definition or NULL if the toolchain hasn't been
  // defined yet.
  const Toolchain* GetToolchainDefinitionUnlocked(const Label& toolchain_name);

  // Sets the default toolchain. If the default toolchain is already set,
  // this function will return false and fill in the given err.
  bool SetDefaultToolchainUnlocked(const Label& dt,
                                   const LocationRange& defined_from,
                                   Err* err);

  // Returns the default toolchain name. This will be empty if it hasn't been
  // set.
  Label GetDefaultToolchainUnlocked() const;

  // Saves the given named toolchain (the name will be taken from the toolchain
  // parameter). This will fail and return false if the given toolchain was
  // already defined. In this case, the given error will be set.
  bool SetToolchainDefinitionLocked(const Toolchain& tc,
                                    const LocationRange& defined_from,
                                    Err* err);

  // Schedules an invocation of the given file under the given toolchain. The
  // toolchain file will be loaded if necessary. If the toolchain is an is_null
  // label, the default toolchain will be used.
  //
  // The origin should be the node that will be blamed for this invocation if
  // an error occurs. If a synchronous error occurs, the given error will be
  // set and it will return false. If an async error occurs, the error will be
  // sent to the scheduler.
  bool ScheduleInvocationLocked(const LocationRange& origin,
                                const Label& toolchain_name,
                                const SourceDir& dir,
                                Err* err);

 private:
  enum SettingsState {
    // Toolchain settings have not requested to be loaded. This means we
    // haven't seen any targets that require this toolchain yet. Not loading
    // the settings automatically allows you to define a bunch of toolchains
    // and potentially not use them without much overhead.
    TOOLCHAIN_SETTINGS_NOT_LOADED,

    // The settings have been scheduled to be loaded but have not completed.
    TOOLCHAIN_SETTINGS_LOADING,

    // The settings are done being loaded.
    TOOLCHAIN_SETTINGS_LOADED
  };

  struct Info;

  static std::string ToolchainToOutputSubdir(const Label& toolchain_name);

  // Creates a new info struct and saves it in the map. A pointer to the
  // struct is returned. No loads are scheduled.
  //
  // If the label is non-empty, the toolchain will be added to the ItemTree
  // so that other nodes can depend on it. THe empty label case is for the
  // default build config file (when the toolchain name isn't known yet). It
  // will be added later.
  //
  // On error, will return NULL and the error will be set.
  Info* LoadNewToolchainLocked(const LocationRange& specified_from,
                               const Label& toolchain_name,
                               Err* err);

  // Fixes up the default toolchain names once they're known when processing
  // the default build config, or throw an error if the default toolchain
  // hasn't been set. See the StartLoading() implementation for more.
  void FixupDefaultToolchainLocked();

  // Loads the base config for the given toolchain. Run on a background thread
  // asynchronously.
  void BackgroundLoadBuildConfig(Info* info,
                                 bool is_default,
                                 const ParseNode* root);

  // Invokes the given file for a toolchain with loaded settings. Run on a
  // background thread asynchronously.
  void BackgroundInvoke(const Info* info,
                        const SourceFile& file_name,
                        const ParseNode* root);

  // Returns the lock to use.
  base::Lock& GetLock() const;

  const BuildSettings* build_settings_;

  // We own the info pointers.
  typedef std::map<Label, Info*> ToolchainMap;
  ToolchainMap toolchains_;

  Label default_toolchain_;
  LocationRange default_toolchain_defined_here_;

  DISALLOW_COPY_AND_ASSIGN(ToolchainManager);
};

#endif  // TOOLS_GN_TOOLCHAIN_MANAGER_H_
