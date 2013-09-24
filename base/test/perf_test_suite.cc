// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/perf_test_suite.h"

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

PerfTestSuite::PerfTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
}

void PerfTestSuite::Initialize() {
  TestSuite::Initialize();

  // Initialize the perf timer log
  FilePath log_path =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath("log-file");
  if (log_path.empty()) {
    FilePath exe;
    PathService::Get(base::FILE_EXE, &exe);
    log_path = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
    log_path = log_path.InsertBeforeExtension(FILE_PATH_LITERAL("_perf"));
  }
  ASSERT_TRUE(InitPerfLog(log_path));

  // Raise to high priority to have more precise measurements. Since we don't
  // aim at 1% precision, it is not necessary to run at realtime level.
  if (!base::debug::BeingDebugged())
    base::RaiseProcessToHighPriority();
}

void PerfTestSuite::Shutdown() {
  TestSuite::Shutdown();
  FinalizePerfLog();
}

}  // namespace base
