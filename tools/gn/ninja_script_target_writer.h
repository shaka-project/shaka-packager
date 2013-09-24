// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_SCRIPT_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_SCRIPT_TARGET_WRITER_H_

#include "base/compiler_specific.h"
#include "tools/gn/ninja_target_writer.h"

// Writes a .ninja file for a custom script target type.
class NinjaScriptTargetWriter : public NinjaTargetWriter {
 public:
  NinjaScriptTargetWriter(const Target* target, std::ostream& out);
  virtual ~NinjaScriptTargetWriter();

  virtual void Run() OVERRIDE;

 private:
  bool has_sources() const { return !target_->sources().empty(); }

  // Writes the Ninja rule for invoking the script.
  //
  // Returns the name of the custom rule generated. This will be based on the
  // target name, and will include the string "$unique_name" if there are
  // multiple inputs.
  std::string WriteRuleDefinition(const std::string& script_relative_to_cd);

  // Writes the rules for compiling each source, writing all output files
  // to the given vector.
  //
  // common_deps is a precomputed string of all ninja files that are common
  // to each build step. This is added to each one.
  void WriteSourceRules(const std::string& custom_rule_name,
                        const std::string& common_deps,
                        const SourceDir& script_cd,
                        const std::string& script_cd_to_root,
                        std::vector<OutputFile>* output_files);

  void WriteArg(const std::string& arg);

  // Writes the .stamp rule that names this target and collects all invocations
  // of the script into one thing that other targets can depend on.
  void WriteStamp(const std::vector<OutputFile>& output_files);

  DISALLOW_COPY_AND_ASSIGN(NinjaScriptTargetWriter);
};

#endif  // TOOLS_GN_NINJA_SCRIPT_TARGET_WRITER_H_
