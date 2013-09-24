// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_build_writer.h"

#include <fstream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/process/process_handle.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/escape.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file_manager.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {

std::string GetSelfInvocationCommand(const BuildSettings* build_settings) {
#if defined(OS_WIN)
  wchar_t module[MAX_PATH];
  GetModuleFileName(NULL, module, MAX_PATH);
  //result = "\"" + WideToUTF8(module) + "\"";
  base::FilePath executable(module);
#elif defined(OS_MACOSX)
  // FIXME(brettw) write this on Mac!
  base::FilePath executable("../Debug/gn");
#else
  base::FilePath executable =
      base::GetProcessExecutablePath(base::GetCurrentProcessHandle());
#endif

/*
  // Append the root path.
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  result += " --root=\"" + FilePathToUTF8(settings->root_path()) + "\"";
*/

  CommandLine cmdline(executable);
  cmdline.AppendSwitchPath("--root", build_settings->root_path());

  // TODO(brettw) append other parameters.

#if defined(OS_WIN)
  return WideToUTF8(cmdline.GetCommandLineString());
#else
  return cmdline.GetCommandLineString();
#endif
}

}  // namespace

NinjaBuildWriter::NinjaBuildWriter(
    const BuildSettings* build_settings,
    const std::vector<const Settings*>& all_settings,
    const std::vector<const Target*>& default_toolchain_targets,
    std::ostream& out)
    : build_settings_(build_settings),
      all_settings_(all_settings),
      default_toolchain_targets_(default_toolchain_targets),
      out_(out),
      path_output_(build_settings->build_dir(), ESCAPE_NINJA, true),
      helper_(build_settings) {
}

NinjaBuildWriter::~NinjaBuildWriter() {
}

void NinjaBuildWriter::Run() {
  WriteNinjaRules();
  WriteSubninjas();
  WritePhonyAndAllRules();
}

// static
bool NinjaBuildWriter::RunAndWriteFile(
    const BuildSettings* build_settings,
    const std::vector<const Settings*>& all_settings,
    const std::vector<const Target*>& default_toolchain_targets) {
  base::FilePath ninja_file(build_settings->GetFullPath(
      SourceFile(build_settings->build_dir().value() + "build.ninja")));
  file_util::CreateDirectory(ninja_file.DirName());

  std::ofstream file;
  file.open(FilePathToUTF8(ninja_file).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail())
    return false;

  NinjaBuildWriter gen(build_settings, all_settings,
                       default_toolchain_targets, file);
  gen.Run();
  return true;
}

void NinjaBuildWriter::WriteNinjaRules() {
  out_ << "rule gn\n";
  out_ << "  command = " << GetSelfInvocationCommand(build_settings_) << "\n";
  out_ << "  description = GN the world\n\n";

  out_ << "build build.ninja: gn";

  // Input build files.
  std::vector<base::FilePath> input_files;
  g_scheduler->input_file_manager()->GetAllPhysicalInputFileNames(&input_files);
  EscapeOptions ninja_options;
  ninja_options.mode = ESCAPE_NINJA;
  for (size_t i = 0; i < input_files.size(); i++)
    out_ << " " << EscapeString(FilePathToUTF8(input_files[i]), ninja_options);

  // Other files read by the build.
  std::vector<base::FilePath> other_files = g_scheduler->GetGenDependencies();
  for (size_t i = 0; i < other_files.size(); i++)
    out_ << " " << EscapeString(FilePathToUTF8(other_files[i]), ninja_options);

  out_ << std::endl << std::endl;
}

void NinjaBuildWriter::WriteSubninjas() {
  for (size_t i = 0; i < all_settings_.size(); i++) {
    out_ << "subninja ";
    path_output_.WriteFile(out_,
                           helper_.GetNinjaFileForToolchain(all_settings_[i]));
    out_ << std::endl;
  }
  out_ << std::endl;
}

void NinjaBuildWriter::WritePhonyAndAllRules() {
  std::string all_rules;

  // Write phony rules for the default toolchain (don't do other toolchains or
  // we'll get naming conflicts).
  for (size_t i = 0; i < default_toolchain_targets_.size(); i++) {
    const Target* target = default_toolchain_targets_[i];

    OutputFile target_file = helper_.GetTargetOutputFile(target);
    if (target_file.value() != target->label().name()) {
      out_ << "build " << target->label().name() << ": phony ";
      path_output_.WriteFile(out_, target_file);
      out_ << std::endl;
    }

    if (!all_rules.empty())
      all_rules.append(" $\n    ");
    all_rules.append(target_file.value());
  }

  if (!all_rules.empty()) {
    out_ << "\nbuild all: phony " << all_rules << std::endl;
    out_ << "default all" << std::endl;
  }
}

