// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_BUILD_SETTINGS_H_
#define TOOLS_GN_BUILD_SETTINGS_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "tools/gn/item_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"
#include "tools/gn/target_manager.h"
#include "tools/gn/toolchain_manager.h"

// Settings for one build, which is one toplevel output directory. There
// may be multiple Settings objects that refer to this, one for each toolchain.
class BuildSettings {
 public:
  typedef base::Callback<void(const Target*)> TargetResolvedCallback;

  BuildSettings();
  ~BuildSettings();

  // Absolute path of the source root on the local system. Everything is
  // relative to this.
  const base::FilePath& root_path() const { return root_path_; }
  void set_root_path(const base::FilePath& r) { root_path_ = r; }

  // When nonempty, specifies a parallel directory higherarchy in which to
  // search for buildfiles if they're not found in the root higherarchy. This
  // allows us to keep buildfiles in a separate tree during development.
  const base::FilePath& secondary_source_path() const {
    return secondary_source_path_;
  }
  void SetSecondarySourcePath(const SourceDir& d);

  // Path of the python executable to run scripts with.
  base::FilePath python_path() const { return python_path_; }
  void set_python_path(const base::FilePath& p) { python_path_ = p; }

  const SourceFile& build_config_file() const { return build_config_file_; }
  void set_build_config_file(const SourceFile& f) { build_config_file_ = f; }

  // The build directory is the root of all output files. The default toolchain
  // files go into here, and non-default toolchains will have separate
  // toolchain-specific root directories inside this.
  const SourceDir& build_dir() const { return build_dir_; }
  void SetBuildDir(const SourceDir& dir);

  // The inverse of relative_build_dir, ending with a separator.
  // Example: relative_build_dir_ = "out/Debug/" this will be "../../"
  const std::string& build_to_source_dir_string() const {
    return build_to_source_dir_string_;
  }

  // These accessors do not return const objects since the resulting objects
  // are threadsafe. In this setting, we use constness primarily to ensure
  // that the Settings object is used in a threadsafe manner. Although this
  // violates the concept of logical constness, that's less important in our
  // application, and actually implementing this in a way that preserves
  // logical constness is cumbersome.
  ItemTree& item_tree() const { return item_tree_; }
  TargetManager& target_manager() const { return target_manager_; }
  ToolchainManager& toolchain_manager() const { return toolchain_manager_; }

  // Returns the full absolute OS path cooresponding to the given file in the
  // root source tree.
  base::FilePath GetFullPath(const SourceFile& file) const;
  base::FilePath GetFullPath(const SourceDir& dir) const;

  // Returns the absolute OS path inside the secondary source path. Will return
  // an empty FilePath if the secondary source path is empty. When loading a
  // buildfile, the GetFullPath should always be consulted first.
  base::FilePath GetFullPathSecondary(const SourceFile& file) const;
  base::FilePath GetFullPathSecondary(const SourceDir& dir) const;

  // This is the callback to execute when a target is marked resolved. If we
  // don't need to do anything, this will be null. When a target is resolved,
  // this callback should be posted to the scheduler pool so the work is
  // distributed properly.
  const TargetResolvedCallback& target_resolved_callback() const {
    return target_resolved_callback_;
  }
  void set_target_resolved_callback(const TargetResolvedCallback& cb) {
    target_resolved_callback_ = cb;
  }

 private:
  base::FilePath root_path_;
  base::FilePath secondary_source_path_;
  base::FilePath python_path_;

  SourceFile build_config_file_;
  SourceDir build_dir_;
  std::string build_to_source_dir_string_;

  TargetResolvedCallback target_resolved_callback_;

  // See getters above.
  mutable ItemTree item_tree_;
  mutable TargetManager target_manager_;
  mutable ToolchainManager toolchain_manager_;

  DISALLOW_COPY_AND_ASSIGN(BuildSettings);
};

#endif  // TOOLS_GN_BUILD_SETTINGS_H_
