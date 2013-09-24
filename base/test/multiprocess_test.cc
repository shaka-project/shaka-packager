// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/multiprocess_test.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace base {

MultiProcessTest::MultiProcessTest() {
}

ProcessHandle MultiProcessTest::SpawnChild(const std::string& procname,
                                           bool debug_on_start) {
  FileHandleMappingVector empty_file_list;
  return SpawnChildImpl(procname, empty_file_list, debug_on_start);
}

#if defined(OS_POSIX)
ProcessHandle MultiProcessTest::SpawnChild(
    const std::string& procname,
    const FileHandleMappingVector& fds_to_map,
    bool debug_on_start) {
  return SpawnChildImpl(procname, fds_to_map, debug_on_start);
}
#endif

CommandLine MultiProcessTest::MakeCmdLine(const std::string& procname,
                                          bool debug_on_start) {
  CommandLine cl(*CommandLine::ForCurrentProcess());
  cl.AppendSwitchASCII(switches::kTestChildProcess, procname);
  if (debug_on_start)
    cl.AppendSwitch(switches::kDebugOnStart);
  return cl;
}

#if !defined(OS_ANDROID)
ProcessHandle MultiProcessTest::SpawnChildImpl(
    const std::string& procname,
    const FileHandleMappingVector& fds_to_map,
    bool debug_on_start) {
  ProcessHandle handle = kNullProcessHandle;
  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#else
  options.fds_to_remap = &fds_to_map;
#endif
  base::LaunchProcess(MakeCmdLine(procname, debug_on_start), options, &handle);
  return handle;
}
#endif  // !defined(OS_ANDROID)

}  // namespace base
