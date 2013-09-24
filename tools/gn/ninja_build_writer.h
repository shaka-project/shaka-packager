// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BUILD_WRITER_H_
#define TOOLS_GN_NINJA_BUILD_WRITER_H_

#include <iosfwd>
#include <vector>

#include "tools/gn/ninja_helper.h"
#include "tools/gn/path_output.h"

class BuildSettings;
class Settings;
class Target;

// Generates the toplevel "build.ninja" file. This references the individual
// toolchain files and lists all input .gn files as dependencies of the
// build itself.
class NinjaBuildWriter {
 public:
  static bool RunAndWriteFile(
      const BuildSettings* settings,
      const std::vector<const Settings*>& all_settings,
      const std::vector<const Target*>& default_toolchain_targets);

 private:
  NinjaBuildWriter(const BuildSettings* settings,
                   const std::vector<const Settings*>& all_settings,
                   const std::vector<const Target*>& default_toolchain_targets,
                   std::ostream& out);
  ~NinjaBuildWriter();

  void Run();

  void WriteNinjaRules();
  void WriteSubninjas();
  void WritePhonyAndAllRules();

  const BuildSettings* build_settings_;
  std::vector<const Settings*> all_settings_;
  std::vector<const Target*> default_toolchain_targets_;
  std::ostream& out_;
  PathOutput path_output_;

  NinjaHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(NinjaBuildWriter);
};

#endif  // TOOLS_GN_NINJA_BUILD_GENERATOR_H_

