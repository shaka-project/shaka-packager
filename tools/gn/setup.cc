// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/setup.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/parser.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"
#include "tools/gn/tokenizer.h"
#include "tools/gn/value.h"

extern const char kDotfile_Help[] =
    ".gn file\n"
    "\n"
    "  When gn starts, it will search the current directory and parent\n"
    "  directories for a file called \".gn\". This indicates the source root.\n"
    "  You can override this detection by using the --root command-line\n"
    "  argument\n"
    "\n"
    "  The .gn file in the source root will be executed. The syntax is the\n"
    "  same as a buildfile, but with very limited build setup-specific\n"
    "  meaning.\n"
    "\n"
    "Variables:\n"
    "  buildconfig [required]\n"
    "      Label of the build config file. This file will be used to setup\n"
    "      the build file execution environment for each toolchain.\n"
    "\n"
    "  secondary_source [optional]\n"
    "      Label of an alternate directory tree to find input files. When\n"
    "      searching for a BUILD.gn file (or the build config file discussed\n"
    "      above), the file fill first be looked for in the source root.\n"
    "      If it's not found, the secondary source root will be checked\n"
    "      (which would contain a parallel directory hierarchy).\n"
    "\n"
    "      This behavior is intended to be used when BUILD.gn files can't be\n"
    "      checked in to certain source directories for whaever reason.\n"
    "\n"
    "      The secondary source root must be inside the main source tree.\n"
    "\n"
    "Example .gn file contents:\n"
    "\n"
    "  buildconfig = \"//build/config/BUILDCONFIG.gn\"\n"
    "\n"
    "  secondary_source = \"//build/config/temporary_buildfiles/\"\n";

namespace {

// More logging.
const char kSwitchVerbose[] = "v";

const char kSwitchRoot[] = "root";
const char kSecondarySource[] = "secondary";

const base::FilePath::CharType kGnFile[] = FILE_PATH_LITERAL(".gn");

base::FilePath FindDotFile(const base::FilePath& current_dir) {
  base::FilePath try_this_file = current_dir.Append(kGnFile);
  if (base::PathExists(try_this_file))
    return try_this_file;

  base::FilePath with_no_slash = current_dir.StripTrailingSeparators();
  base::FilePath up_one_dir = with_no_slash.DirName();
  if (up_one_dir == current_dir)
    return base::FilePath();  // Got to the top.

  return FindDotFile(up_one_dir);
}

}  // namespace

Setup::Setup()
    : dotfile_toolchain_(Label()),
      dotfile_settings_(&dotfile_build_settings_, &dotfile_toolchain_,
                        std::string()),
      dotfile_scope_(&dotfile_settings_) {
}

Setup::~Setup() {
}

bool Setup::DoSetup() {
  CommandLine* cmdline = CommandLine::ForCurrentProcess();

  scheduler_.set_verbose_logging(cmdline->HasSwitch(kSwitchVerbose));

  if (!FillSourceDir(*cmdline))
    return false;
  if (!RunConfigFile())
    return false;
  if (!FillOtherConfig(*cmdline))
    return false;

  // FIXME(brettw) get python path!
#if defined(OS_WIN)
  build_settings_.set_python_path(
      base::FilePath(FILE_PATH_LITERAL("cmd.exe /c python")));
#else
  build_settings_.set_python_path(base::FilePath(FILE_PATH_LITERAL("python")));
#endif

  build_settings_.SetBuildDir(SourceDir("//out/gn/"));

  return true;
}

bool Setup::Run() {
  // Load the root build file and start runnung.
  build_settings_.toolchain_manager().StartLoadingUnlocked(
      SourceFile("//BUILD.gn"));
  if (!scheduler_.Run())
    return false;

  Err err = build_settings_.item_tree().CheckForBadItems();
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }
  return true;
}

bool Setup::FillSourceDir(const CommandLine& cmdline) {
  // Find the .gn file.
  base::FilePath root_path;

  // Prefer the command line args to the config file.
  base::FilePath relative_root_path = cmdline.GetSwitchValuePath(kSwitchRoot);
  if (!relative_root_path.empty()) {
    root_path = base::MakeAbsoluteFilePath(relative_root_path);
    dotfile_name_ = root_path.Append(kGnFile);
  } else {
    base::FilePath cur_dir;
    file_util::GetCurrentDirectory(&cur_dir);
    dotfile_name_ = FindDotFile(cur_dir);
    if (dotfile_name_.empty()) {
      Err(Location(), "Can't find source root.",
          "I could not find a \".gn\" file in the current directory or any "
          "parent,\nand the --root command-line argument was not specified.")
          .PrintToStdout();
      return false;
    }
    root_path = dotfile_name_.DirName();
  }

  if (scheduler_.verbose_logging())
    scheduler_.Log("Using source root", FilePathToUTF8(root_path));
  build_settings_.set_root_path(root_path);

  return true;
}

bool Setup::RunConfigFile() {
  if (scheduler_.verbose_logging())
    scheduler_.Log("Got dotfile", FilePathToUTF8(dotfile_name_));

  dotfile_input_file_.reset(new InputFile(SourceFile("//.gn")));
  if (!dotfile_input_file_->Load(dotfile_name_)) {
    Err(Location(), "Could not load dotfile.",
        "The file \"" + FilePathToUTF8(dotfile_name_) + "\" cound't be loaded")
        .PrintToStdout();
    return false;
  }

  Err err;
  dotfile_tokens_ = Tokenizer::Tokenize(dotfile_input_file_.get(), &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  dotfile_root_ = Parser::Parse(dotfile_tokens_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  dotfile_root_->AsBlock()->ExecuteBlockInScope(&dotfile_scope_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  return true;
}

bool Setup::FillOtherConfig(const CommandLine& cmdline) {
  Err err;

  // Secondary source path.
  SourceDir secondary_source;
  if (cmdline.HasSwitch(kSecondarySource)) {
    // Prefer the command line over the config file.
    secondary_source =
        SourceDir(cmdline.GetSwitchValueASCII(kSecondarySource));
  } else {
    // Read from the config file if present.
    const Value* secondary_value =
        dotfile_scope_.GetValue("secondary_source", true);
    if (secondary_value) {
      if (!secondary_value->VerifyTypeIs(Value::STRING, &err)) {
        err.PrintToStdout();
        return false;
      }
      build_settings_.SetSecondarySourcePath(
          SourceDir(secondary_value->string_value()));
    }
  }

  // Build config file.
  const Value* build_config_value =
      dotfile_scope_.GetValue("buildconfig", true);
  if (!build_config_value) {
    Err(Location(), "No build config file.",
        "Your .gn file (\"" + FilePathToUTF8(dotfile_name_) + "\")\n"
        "didn't specify a \"buildconfig\" value.").PrintToStdout();
    return false;
  } else if (!build_config_value->VerifyTypeIs(Value::STRING, &err)) {
    err.PrintToStdout();
    return false;
  }
  build_settings_.set_build_config_file(
      SourceFile(build_config_value->string_value()));

  return true;
}
