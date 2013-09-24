// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/logging.h"

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  struct kinfo_proc info;
  size_t length = sizeof(struct kinfo_proc);
  int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, process };
  if (sysctl(mib, 4, &info, &length, NULL, 0) < 0) {
    DPLOG(ERROR) << "sysctl";
    return -1;
  }
  if (length == 0)
    return -1;
  return info.kp_eproc.e_ppid;
}

}  // namespace base
