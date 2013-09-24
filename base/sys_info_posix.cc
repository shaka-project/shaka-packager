// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"

#if defined(OS_ANDROID)
#include <sys/vfs.h>
#define statvfs statfs  // Android uses a statvfs-like statfs struct and call.
#else
#include <sys/statvfs.h>
#endif

namespace base {

#if !defined(OS_OPENBSD)
int SysInfo::NumberOfProcessors() {
  // It seems that sysconf returns the number of "logical" processors on both
  // Mac and Linux.  So we get the number of "online logical" processors.
  long res = sysconf(_SC_NPROCESSORS_ONLN);
  if (res == -1) {
    NOTREACHED();
    return 1;
  }

  return static_cast<int>(res);
}
#endif

// static
int64 SysInfo::AmountOfFreeDiskSpace(const FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  struct statvfs stats;
  if (HANDLE_EINTR(statvfs(path.value().c_str(), &stats)) != 0)
    return -1;
  return static_cast<int64>(stats.f_bavail) * stats.f_frsize;
}

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
// static
std::string SysInfo::OperatingSystemName() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }
  return std::string(info.sysname);
}
#endif

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
// static
std::string SysInfo::OperatingSystemVersion() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }
  return std::string(info.release);
}
#endif

// static
std::string SysInfo::OperatingSystemArchitecture() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return std::string();
  }
  std::string arch(info.machine);
  if (arch == "i386" || arch == "i486" || arch == "i586" || arch == "i686") {
    arch = "x86";
  } else if (arch == "amd64") {
    arch = "x86_64";
  }
  return arch;
}

// static
size_t SysInfo::VMAllocationGranularity() {
  return getpagesize();
}

}  // namespace base
