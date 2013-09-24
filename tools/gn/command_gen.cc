// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/commands.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/ninja_writer.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"

namespace commands {

namespace {

// Suppress output on success.
const char kSwitchQuiet[] = "q";

void TargetResolvedCallback(base::subtle::Atomic32* write_counter,
                            const Target* target) {
  base::subtle::NoBarrier_AtomicIncrement(write_counter, 1);
  NinjaTargetWriter::RunAndWriteFile(target);
}

}  // namespace

const char kGen[] = "gen";
const char kGen_HelpShort[] =
    "gen: Generate ninja files.";
const char kGen_Help[] =
    "gn gen\n"
    "  Generates ninja files from the current tree.\n"
    "\n"
    "  See \"gn help\" for the common command-line switches.\n";

int RunGen(const std::vector<std::string>& args) {
  base::TimeTicks begin_time = base::TimeTicks::Now();

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  if (!setup->DoSetup())
    return 1;

  // Cause the load to also generate the ninja files for each target. We wrap
  // the writing to maintain a counter.
  base::subtle::Atomic32 write_counter = 0;
  setup->build_settings().set_target_resolved_callback(
      base::Bind(&TargetResolvedCallback, &write_counter));

  // Do the actual load. This will also write out the target ninja files.
  if (!setup->Run())
    return 1;

  // Write the root ninja files.
  if (!NinjaWriter::RunAndWriteFiles(&setup->build_settings()))
    return 1;

  base::TimeTicks end_time = base::TimeTicks::Now();

  if (!CommandLine::ForCurrentProcess()->HasSwitch(kSwitchQuiet)) {
    OutputString("Done. ", DECORATION_GREEN);

    std::string stats = "Wrote " +
        base::IntToString(static_cast<int>(write_counter)) +
        " targets from " +
        base::IntToString(
            setup->scheduler().input_file_manager()->GetInputFileCount()) +
        " files in " +
        base::IntToString((end_time - begin_time).InMilliseconds()) + "ms\n";
    OutputString(stats);
  }

  return 0;
}

}  // namespace commands
