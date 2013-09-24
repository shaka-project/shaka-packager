// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SCRIPT_VALUES_H_
#define TOOLS_GN_SCRIPT_VALUES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "tools/gn/source_file.h"

// Holds the values (outputs, args, script name, etc.) for a script-based
// target.
class ScriptValues {
 public:
  ScriptValues();
  ~ScriptValues();

  // Filename of the script to execute.
  const SourceFile& script() const { return script_; }
  void set_script(const SourceFile& s) { script_ = s; }

  // Arguments to the script.
  const std::vector<std::string>& args() const { return args_; }
  void swap_in_args(std::vector<std::string>* a) { args_.swap(*a); }

  // Files created by the script.
  const std::vector<SourceFile>& outputs() const { return outputs_; }
  void swap_in_outputs(std::vector<SourceFile>* op) { outputs_.swap(*op); }

 private:
  SourceFile script_;
  std::vector<std::string> args_;
  std::vector<SourceFile> outputs_;

  DISALLOW_COPY_AND_ASSIGN(ScriptValues);
};

#endif  // TOOLS_GN_SCRIPT_VALUES_H_
