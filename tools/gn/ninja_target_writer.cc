// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_target_writer.h"

#include <fstream>
#include <sstream>

#include "base/file_util.h"
#include "tools/gn/err.h"
#include "tools/gn/ninja_binary_target_writer.h"
#include "tools/gn/ninja_copy_target_writer.h"
#include "tools/gn/ninja_group_target_writer.h"
#include "tools/gn/ninja_script_target_writer.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"

NinjaTargetWriter::NinjaTargetWriter(const Target* target, std::ostream& out)
    : settings_(target->settings()),
      target_(target),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   ESCAPE_NINJA, true),
      helper_(settings_->build_settings()) {
}

NinjaTargetWriter::~NinjaTargetWriter() {
}

// static
void NinjaTargetWriter::RunAndWriteFile(const Target* target) {
  const Settings* settings = target->settings();
  NinjaHelper helper(settings->build_settings());

  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      helper.GetNinjaFileForTarget(target).GetSourceFile(
          settings->build_settings())));

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Writing", FilePathToUTF8(ninja_file));

  file_util::CreateDirectory(ninja_file.DirName());

  // It's rediculously faster to write to a string and then write that to
  // disk in one operation than to use an fstream here.
  std::stringstream file;
  if (file.fail()) {
    g_scheduler->FailWithError(
        Err(Location(), "Error writing ninja file.",
            "Unable to open \"" + FilePathToUTF8(ninja_file) + "\"\n"
            "for writing."));
    return;
  }

  // Call out to the correct sub-type of writer.
  if (target->output_type() == Target::COPY_FILES) {
    NinjaCopyTargetWriter writer(target, file);
    writer.Run();
  } else if (target->output_type() == Target::CUSTOM) {
    NinjaScriptTargetWriter writer(target, file);
    writer.Run();
  } else if (target->output_type() == Target::GROUP) {
    NinjaGroupTargetWriter writer(target, file);
    writer.Run();
  } else if (target->output_type() == Target::EXECUTABLE ||
             target->output_type() == Target::STATIC_LIBRARY ||
             target->output_type() == Target::SHARED_LIBRARY) {
    NinjaBinaryTargetWriter writer(target, file);
    writer.Run();
  } else {
    CHECK(0);
  }

  std::string contents = file.str();
  file_util::WriteFile(ninja_file, contents.c_str(), contents.size());
}

void NinjaTargetWriter::WriteEnvironment() {
  // TODO(brettw) have a better way to do the environment setup on Windows.
  if (target_->settings()->IsWin())
    out_ << "arch = environment.x86\n";
}
