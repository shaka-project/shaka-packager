// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_toolchain_writer.h"

#include <fstream>

#include "base/file_util.h"
#include "base/strings/stringize_macros.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"

NinjaToolchainWriter::NinjaToolchainWriter(
    const Settings* settings,
    const std::vector<const Target*>& targets,
    std::ostream& out)
    : settings_(settings),
      targets_(targets),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   ESCAPE_NINJA, true),
      helper_(settings->build_settings()) {
}

NinjaToolchainWriter::~NinjaToolchainWriter() {
}

void NinjaToolchainWriter::Run() {
  WriteRules();
  WriteSubninjas();
}

// static
bool NinjaToolchainWriter::RunAndWriteFile(
    const Settings* settings,
    const std::vector<const Target*>& targets) {
  NinjaHelper helper(settings->build_settings());
  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      helper.GetNinjaFileForToolchain(settings).GetSourceFile(
          settings->build_settings())));
  file_util::CreateDirectory(ninja_file.DirName());

  std::ofstream file;
  file.open(FilePathToUTF8(ninja_file).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail())
    return false;

  NinjaToolchainWriter gen(settings, targets, file);
  gen.Run();
  return true;
}

void NinjaToolchainWriter::WriteRules() {
  const Toolchain* tc = settings_->toolchain();
  std::string indent("  ");

  for (int i = Toolchain::TYPE_NONE + 1; i < Toolchain::TYPE_NUMTYPES; i++) {
    Toolchain::ToolType tool_type = static_cast<Toolchain::ToolType>(i);
    const Toolchain::Tool& tool = tc->GetTool(tool_type);
    if (tool.empty())
      continue;

    out_ << "rule " << Toolchain::ToolTypeToName(tool_type) << std::endl;

    #define WRITE_ARG(name) \
      if (!tool.name.empty()) \
        out_ << indent << "  " STRINGIZE(name) " = " << tool.name << std::endl;
    WRITE_ARG(command);
    WRITE_ARG(depfile);
    WRITE_ARG(deps);
    WRITE_ARG(description);
    WRITE_ARG(pool);
    WRITE_ARG(restat);
    WRITE_ARG(rspfile);
    WRITE_ARG(rspfile_content);
    #undef WRITE_ARG
  }
  out_ << std::endl;
}

void NinjaToolchainWriter::WriteSubninjas() {
  for (size_t i = 0; i < targets_.size(); i++) {
    out_ << "subninja ";
    path_output_.WriteFile(out_, helper_.GetNinjaFileForTarget(targets_[i]));
    out_ << std::endl;
  }
  out_ << std::endl;
}
