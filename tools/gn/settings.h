// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SETTINGS_H_
#define TOOLS_GN_SETTINGS_H_

#include "base/files/file_path.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/import_manager.h"
#include "tools/gn/output_file.h"
#include "tools/gn/scope.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/toolchain.h"

// Holds the settings for one toolchain invocation. There will be one
// Settings object for each toolchain type, each referring to the same
// BuildSettings object for shared stuff.
//
// The Settings object is const once it is constructed, which allows us to
// use it from multiple threads during target generation without locking (which
// is important, because it gets used a lot).
//
// The Toolchain object holds the set of stuff that is set by the toolchain
// declaration, which obviously needs to be set later when we actually parse
// the file with the toolchain declaration in it.
class Settings {
 public:
  enum TargetOS {
    UNKNOWN,
    LINUX,
    MAC,
    WIN
  };

  // Constructs a toolchain settings. The output_subdir_name is the name we
  // should use for the subdirectory in the build output directory for this
  // toolchain's outputs. It should have no slashes in it. The default
  // toolchain should use an empty string.
  Settings(const BuildSettings* build_settings,
           const Toolchain* toolchain,
           const std::string& output_subdir_name);
  ~Settings();

  const BuildSettings* build_settings() const { return build_settings_; }

  // Danger: this must only be used for getting the toolchain label until the
  // toolchain has been resolved. Otherwise, it will be modified on an
  // arbitrary thread when the toolchain invocation is found. Generally, you
  // will only read this from the target generation where we know everything
  // has been resolved and won't change.
  const Toolchain* toolchain() const { return toolchain_; }

  bool IsMac() const { return target_os_ == MAC; }
  bool IsLinux() const { return target_os_ == LINUX; }
  bool IsWin() const { return target_os_ == WIN; }

  TargetOS target_os() const { return target_os_; }
  void set_target_os(TargetOS t) { target_os_ = t; }

  const OutputFile& toolchain_output_subdir() const {
    return toolchain_output_subdir_;
  }
  const SourceDir& toolchain_output_dir() const {
    return toolchain_output_dir_;
  }

  // Directory for generated files.
  const SourceDir& toolchain_gen_dir() const {
    return toolchain_gen_dir_;
  }

  // The import manager caches the result of executing imported files in the
  // context of a given settings object.
  //
  // See the ItemTree getter in GlobalSettings for why this doesn't return a
  // const pointer.
  ImportManager& import_manager() const { return import_manager_; }

  const Scope* base_config() const { return &base_config_; }
  Scope* base_config() { return &base_config_; }

  // Set to true when every target we encounter should be generated. False
  // means that only targets that have a dependency from (directly or
  // indirectly) some magic root node are actually generated. See the comments
  // on ItemTree for more.
  bool greedy_target_generation() const {
    return greedy_target_generation_;
  }
  void set_greedy_target_generation(bool gtg) {
    greedy_target_generation_ = gtg;
  }

 private:
  const BuildSettings* build_settings_;

  const Toolchain* toolchain_;

  TargetOS target_os_;

  mutable ImportManager import_manager_;

  // The subdirectory inside the build output for this toolchain. For the
  // default toolchain, this will be empty (since the deafult toolchain's
  // output directory is the same as the build directory). When nonempty, this
  // is guaranteed to end in a slash.
  OutputFile toolchain_output_subdir_;

  // Full source file path to the toolchain output directory.
  SourceDir toolchain_output_dir_;

  SourceDir toolchain_gen_dir_;

  Scope base_config_;

  bool greedy_target_generation_;

  DISALLOW_COPY_AND_ASSIGN(Settings);
};

#endif  // TOOLS_GN_SETTINGS_H_
