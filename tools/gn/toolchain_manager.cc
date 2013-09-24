// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/toolchain_manager.h"

#include <set>

#include "base/bind.h"
#include "build/build_config.h"
#include "tools/gn/err.h"
#include "tools/gn/item.h"
#include "tools/gn/item_node.h"
#include "tools/gn/item_tree.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/scope_per_file_provider.h"

namespace {

SourceFile DirToBuildFile(const SourceDir& dir) {
  return SourceFile(dir.value() + "BUILD.gn");
}

void SetSystemVars(const Settings& settings, Scope* scope) {
#if defined(OS_WIN)
  scope->SetValue("is_win", Value(NULL, 1), NULL);
  scope->SetValue("is_posix", Value(NULL, 0), NULL);
#else
  scope->SetValue("is_win", Value(NULL, 0), NULL);
  scope->SetValue("is_posix", Value(NULL, 1), NULL);
#endif

#if defined(OS_MACOSX)
  scope->SetValue("is_mac", Value(NULL, 1), NULL);
#else
  scope->SetValue("is_mac", Value(NULL, 0), NULL);
#endif

#if defined(OS_LINUX)
  scope->SetValue("is_linux", Value(NULL, 1), NULL);
#else
  scope->SetValue("is_linux", Value(NULL, 0), NULL);
#endif
}

}  // namespace

struct ToolchainManager::Info {
  Info(const BuildSettings* build_settings,
       const Label& toolchain_name,
       const std::string& output_subdir_name)
      : state(TOOLCHAIN_SETTINGS_NOT_LOADED),
        toolchain(toolchain_name),
        toolchain_set(false),
        settings(build_settings, &toolchain, output_subdir_name),
        item_node(NULL) {
  }

  // Makes sure that an ItemNode is created for the toolchain, which lets
  // targets depend on the (potentially future) loading of the toolchain.
  //
  // We can't always do this at the beginning since when doing the default
  // build config, we don't know the toolchain name yet. We also need to go
  // through some effort to avoid doing this inside the toolchain manager's
  // lock (to avoid holding two locks at once).
  void EnsureItemNode() {
    if (!item_node) {
      ItemTree& tree = settings.build_settings()->item_tree();
      item_node = new ItemNode(&toolchain);
      tree.AddNodeLocked(item_node);
    }
  }

  SettingsState state;

  Toolchain toolchain;
  bool toolchain_set;
  LocationRange toolchain_definition_location;

  // When the state is TOOLCHAIN_SETTINGS_LOADED, the settings should be
  // considered read-only and can be read without locking. Otherwise, they
  // should not be accessed at all except to load them (which can therefore
  // also be done outside of the lock). This works as long as the state flag
  // is only ever read or written inside the lock.
  Settings settings;

  // While state == TOOLCHAIN_SETTINGS_LOADING, this will collect all
  // scheduled invocations using this toolchain. They'll be issued once the
  // settings file has been interpreted.
  //
  // The map maps the source file to "some" location it was invoked from (so
  // we can give good error messages. It does NOT map to the root of the
  // file to be invoked (the file still needs loading). This will be NULL
  // for internally invoked files.
  typedef std::map<SourceFile, LocationRange> ScheduledInvocationMap;
  ScheduledInvocationMap scheduled_invocations;

  // Tracks all scheduled and executed invocations for this toolchain. This
  // is used to avoid invoking a file more than once for a toolchain.
  std::set<SourceFile> all_invocations;

  // Filled in by EnsureItemNode, see that for more.
  ItemNode* item_node;
};

ToolchainManager::ToolchainManager(const BuildSettings* build_settings)
    : build_settings_(build_settings) {
}

ToolchainManager::~ToolchainManager() {
  for (ToolchainMap::iterator i = toolchains_.begin();
       i != toolchains_.end(); ++i)
    delete i->second;
  toolchains_.clear();
}

void ToolchainManager::StartLoadingUnlocked(const SourceFile& build_file_name) {
  // How the default build config works: Initially we don't have a toolchain
  // name to call the settings for the default build config. So we create one
  // with an empty toolchain name and execute the default build config file.
  // When that's done, we'll go and fix up the name to the default build config
  // that the script set.
  base::AutoLock lock(GetLock());
  Err err;
  Info* info = LoadNewToolchainLocked(LocationRange(), Label(), &err);
  if (err.has_error())
    g_scheduler->FailWithError(err);
  CHECK(info);
  info->scheduled_invocations[build_file_name] = LocationRange();
  info->all_invocations.insert(build_file_name);

  g_scheduler->IncrementWorkCount();
  if (!g_scheduler->input_file_manager()->AsyncLoadFile(
           LocationRange(), build_settings_,
           build_settings_->build_config_file(),
           base::Bind(&ToolchainManager::BackgroundLoadBuildConfig,
                      base::Unretained(this), info, true),
           &err)) {
    g_scheduler->FailWithError(err);
    g_scheduler->DecrementWorkCount();
  }
}

const Settings* ToolchainManager::GetSettingsForToolchainLocked(
    const LocationRange& from_here,
    const Label& toolchain_name,
    Err* err) {
  GetLock().AssertAcquired();
  ToolchainMap::iterator found = toolchains_.find(toolchain_name);
  Info* info = NULL;
  if (found == toolchains_.end()) {
    info = LoadNewToolchainLocked(from_here, toolchain_name, err);
    if (!info)
      return NULL;
  } else {
    info = found->second;
  }
  info->EnsureItemNode();

  return &info->settings;
}

const Toolchain* ToolchainManager::GetToolchainDefinitionUnlocked(
    const Label& toolchain_name) {
  base::AutoLock lock(GetLock());
  ToolchainMap::iterator found = toolchains_.find(toolchain_name);
  if (found == toolchains_.end() || !found->second->toolchain_set)
    return NULL;

  // Since we don't allow defining a toolchain more than once, we know that
  // once it's set it won't be mutated, so we can safely return this pointer
  // for reading outside the lock.
  return &found->second->toolchain;
}

bool ToolchainManager::SetDefaultToolchainUnlocked(
    const Label& default_toolchain,
    const LocationRange& defined_here,
    Err* err) {
  base::AutoLock lock(GetLock());
  if (!default_toolchain_.is_null()) {
    *err = Err(defined_here, "Default toolchain already set.");
    err->AppendSubErr(Err(default_toolchain_defined_here_,
                          "Previously defined here.",
                          "You can only set this once."));
    return false;
  }

  if (default_toolchain.is_null()) {
    *err = Err(defined_here, "Bad default toolchain name.",
        "You can't set the default toolchain name to nothing.");
    return false;
  }
  if (!default_toolchain.toolchain_dir().is_null() ||
      !default_toolchain.toolchain_name().empty()) {
    *err = Err(defined_here, "Toolchain name has toolchain.",
        "You can't specify a toolchain (inside the parens) for a toolchain "
        "name. I got:\n" + default_toolchain.GetUserVisibleName(true));
    return false;
  }

  default_toolchain_ = default_toolchain;
  default_toolchain_defined_here_ = defined_here;
  return true;
}

Label ToolchainManager::GetDefaultToolchainUnlocked() const {
  base::AutoLock lock(GetLock());
  return default_toolchain_;
}

bool ToolchainManager::SetToolchainDefinitionLocked(
    const Toolchain& tc,
    const LocationRange& defined_from,
    Err* err) {
  GetLock().AssertAcquired();

  ToolchainMap::iterator found = toolchains_.find(tc.label());
  Info* info = NULL;
  if (found == toolchains_.end()) {
    // New toolchain.
    info = LoadNewToolchainLocked(defined_from, tc.label(), err);
    if (!info)
      return false;
  } else {
    // It's important to preserve the exact Toolchain object in our tree since
    // it will be in the ItemTree and targets may have dependencies on it.
    info = found->second;
  }

  // The labels should match or else we're setting the wrong one!
  CHECK(info->toolchain.label() == tc.label());

  info->toolchain = tc;
  if (info->toolchain_set) {
    *err = Err(defined_from, "Duplicate toolchain definition.");
    err->AppendSubErr(Err(
        info->toolchain_definition_location,
        "Previously defined here.",
        "A toolchain can only be defined once. One tricky way that this could\n"
        "happen is if your definition is itself in a file that's interpreted\n"
        "under different toolchains, which would result in multiple\n"
        "definitions as the file is loaded multiple times. So be sure your\n"
        "toolchain definitions are in files that either don't define any\n"
        "targets (probably best) or at least don't contain targets executed\n"
        "with more than one toolchain."));
    return false;
  }

  info->EnsureItemNode();

  info->toolchain_set = true;
  info->toolchain_definition_location = defined_from;
  return true;
}

bool ToolchainManager::ScheduleInvocationLocked(
    const LocationRange& specified_from,
    const Label& toolchain_name,
    const SourceDir& dir,
    Err* err) {
  GetLock().AssertAcquired();
  SourceFile build_file(DirToBuildFile(dir));

  // If there's no specified toolchain name, use the default.
  ToolchainMap::iterator found;
  if (toolchain_name.is_null())
    found = toolchains_.find(default_toolchain_);
  else
    found = toolchains_.find(toolchain_name);

  Info* info = NULL;
  if (found == toolchains_.end()) {
    // New toolchain.
    info = LoadNewToolchainLocked(specified_from, toolchain_name, err);
    if (!info)
      return false;
  } else {
    // Use existing one.
    info = found->second;
    if (info->all_invocations.find(build_file) !=
        info->all_invocations.end()) {
      // We've already seen this source file for this toolchain, don't need
      // to do anything.
      return true;
    }
  }

  info->all_invocations.insert(build_file);

  // True if the settings load needs to be scheduled.
  bool info_needs_settings_load = false;

  // True if the settings load has completed.
  bool info_settings_loaded = false;

  switch (info->state) {
    case TOOLCHAIN_SETTINGS_NOT_LOADED:
      info_needs_settings_load = true;
      info->scheduled_invocations[build_file] = specified_from;
      break;

    case TOOLCHAIN_SETTINGS_LOADING:
      info->scheduled_invocations[build_file] = specified_from;
      break;

    case TOOLCHAIN_SETTINGS_LOADED:
      info_settings_loaded = true;
      break;
  }

  if (info_needs_settings_load) {
    // Load the settings file.
    g_scheduler->IncrementWorkCount();
    if (!g_scheduler->input_file_manager()->AsyncLoadFile(
             specified_from, build_settings_,
             build_settings_->build_config_file(),
             base::Bind(&ToolchainManager::BackgroundLoadBuildConfig,
                        base::Unretained(this), info, false),
             err)) {
      g_scheduler->DecrementWorkCount();
      return false;
    }
  } else if (info_settings_loaded) {
    // Settings are ready to go, load the target file.
    g_scheduler->IncrementWorkCount();
    if (!g_scheduler->input_file_manager()->AsyncLoadFile(
             specified_from, build_settings_, build_file,
             base::Bind(&ToolchainManager::BackgroundInvoke,
                        base::Unretained(this), info, build_file),
             err)) {
      g_scheduler->DecrementWorkCount();
      return false;
    }
  }
  // Else we should have added the infocations to the scheduled_invocations
  // from within the lock above.
  return true;
}

// static
std::string ToolchainManager::ToolchainToOutputSubdir(
    const Label& toolchain_name) {
  // For now just assume the toolchain name is always a valid dir name. We may
  // want to clean up the in the future.
  return toolchain_name.name();
}

ToolchainManager::Info* ToolchainManager::LoadNewToolchainLocked(
    const LocationRange& specified_from,
    const Label& toolchain_name,
    Err* err) {
  GetLock().AssertAcquired();
  Info* info = new Info(build_settings_,
                        toolchain_name,
                        ToolchainToOutputSubdir(toolchain_name));

  toolchains_[toolchain_name] = info;

  // Invoke the file containing the toolchain definition so that it gets
  // defined. The default one (label is empty) will be done spearately.
  if (!toolchain_name.is_null()) {
    // The default toolchain should be specified whenever we're requesting
    // another one. This is how we know under what context we should execute
    // the invoke for the toolchain file.
    CHECK(!default_toolchain_.is_null());
    ScheduleInvocationLocked(specified_from, default_toolchain_,
                             toolchain_name.dir(), err);
  }
  return info;
}

void ToolchainManager::FixupDefaultToolchainLocked() {
  // Now that we've run the default build config, we should know the
  // default toolchain name. Fix up our reference.
  // See Start() for more.
  GetLock().AssertAcquired();
  if (default_toolchain_.is_null()) {
    g_scheduler->FailWithError(Err(Location(),
        "Default toolchain not set.",
        "Your build config file \"" +
        build_settings_->build_config_file().value() +
        "\"\ndid not call set_default_toolchain(). This is needed so "
        "I know how to actually\ncompile your code."));
    return;
  }

  ToolchainMap::iterator old_default = toolchains_.find(Label());
  CHECK(old_default != toolchains_.end());
  Info* info = old_default->second;
  toolchains_[default_toolchain_] = info;
  toolchains_.erase(old_default);

  // Toolchain should not have been loaded in the build config file.
  CHECK(!info->toolchain_set);

  // We need to set the toolchain label now that we know it. There's no way
  // to set the label, but we can assign the toolchain to a new one. Loading
  // the build config can not change the toolchain, so we won't be overwriting
  // anything useful.
  info->toolchain = Toolchain(default_toolchain_);
  info->EnsureItemNode();

  // The default toolchain is loaded in greedy mode so all targets we
  // encounter are generated. Non-default toolchain settings stay in non-greedy
  // so we only generate the minimally required set.
  info->settings.set_greedy_target_generation(true);

  // Schedule a load of the toolchain build file.
  Err err;
  ScheduleInvocationLocked(LocationRange(), default_toolchain_,
                           default_toolchain_.dir(), &err);
  if (err.has_error())
    g_scheduler->FailWithError(err);
}

void ToolchainManager::BackgroundLoadBuildConfig(Info* info,
                                                 bool is_default,
                                                 const ParseNode* root) {
  // Danger: No early returns without decrementing the work count.
  if (root && !g_scheduler->is_failed()) {
    // Nobody should be accessing settings at this point other than us since we
    // haven't marked it loaded, so we can do it outside the lock.
    Scope* base_config = info->settings.base_config();
    SetSystemVars(info->settings, base_config);
    base_config->SetProcessingBuildConfig();
    if (is_default)
      base_config->SetProcessingDefaultBuildConfig();

    const BlockNode* root_block = root->AsBlock();
    Err err;
    root_block->ExecuteBlockInScope(base_config, &err);

    base_config->ClearProcessingBuildConfig();
    if (is_default)
      base_config->ClearProcessingDefaultBuildConfig();

    if (err.has_error()) {
      g_scheduler->FailWithError(err);
    } else {
      // Base config processing succeeded.
      Info::ScheduledInvocationMap schedule_these;
      {
        base::AutoLock lock(GetLock());
        schedule_these.swap(info->scheduled_invocations);
        info->state = TOOLCHAIN_SETTINGS_LOADED;
        if (is_default)
          FixupDefaultToolchainLocked();
      }

      // Schedule build files waiting on this settings. There can be many so we
      // want to load them in parallel on the pool.
      for (Info::ScheduledInvocationMap::iterator i = schedule_these.begin();
           i != schedule_these.end() && !g_scheduler->is_failed(); ++i) {
        // Note i->second may be NULL, so don't dereference.
        g_scheduler->IncrementWorkCount();
        if (!g_scheduler->input_file_manager()->AsyncLoadFile(
                 i->second, build_settings_, i->first,
                 base::Bind(&ToolchainManager::BackgroundInvoke,
                            base::Unretained(this), info, i->first),
                 &err)) {
          g_scheduler->FailWithError(err);
          g_scheduler->DecrementWorkCount();
          break;
        }
      }
    }
  }
  g_scheduler->DecrementWorkCount();
}

void ToolchainManager::BackgroundInvoke(const Info* info,
                                        const SourceFile& file_name,
                                        const ParseNode* root) {
  if (root && !g_scheduler->is_failed()) {
    if (g_scheduler->verbose_logging()) {
      g_scheduler->Log("Running", file_name.value() + " with toolchain " +
                       info->toolchain.label().GetUserVisibleName(false));
    }

    Scope our_scope(info->settings.base_config());
    ScopePerFileProvider per_file_provider(&our_scope, file_name);

    Err err;
    root->Execute(&our_scope, &err);
    if (err.has_error())
      g_scheduler->FailWithError(err);
  }

  g_scheduler->DecrementWorkCount();
}

base::Lock& ToolchainManager::GetLock() const {
  return build_settings_->item_tree().lock();
}
