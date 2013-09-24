// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/proc_maps_linux.h"

#if defined(OS_LINUX)
#include <inttypes.h>
#endif

#include "base/file_util.h"
#include "base/strings/string_split.h"

#if defined(OS_ANDROID)
// Bionic's inttypes.h defines PRI/SCNxPTR as an unsigned long int, which
// is incompatible with Bionic's stdint.h defining uintptr_t as a unsigned int:
// https://code.google.com/p/android/issues/detail?id=57218
#undef SCNxPTR
#define SCNxPTR "x"
#endif

namespace base {
namespace debug {

bool ReadProcMaps(std::string* proc_maps) {
  FilePath proc_maps_path("/proc/self/maps");
  return file_util::ReadFileToString(proc_maps_path, proc_maps);
}

bool ParseProcMaps(const std::string& input,
                   std::vector<MappedMemoryRegion>* regions_out) {
  std::vector<MappedMemoryRegion> regions;

  // This isn't async safe nor terribly efficient, but it doesn't need to be at
  // this point in time.
  std::vector<std::string> lines;
  SplitString(input, '\n', &lines);

  for (size_t i = 0; i < lines.size(); ++i) {
    // Due to splitting on '\n' the last line should be empty.
    if (i == lines.size() - 1) {
      if (!lines[i].empty())
        return false;
      break;
    }

    MappedMemoryRegion region;
    const char* line = lines[i].c_str();
    char permissions[5] = {'\0'};  // Ensure NUL-terminated string.
    uint8 dev_major = 0;
    uint8 dev_minor = 0;
    long inode = 0;
    int path_index = 0;

    // Sample format from man 5 proc:
    //
    // address           perms offset  dev   inode   pathname
    // 08048000-08056000 r-xp 00000000 03:0c 64593   /usr/sbin/gpm
    //
    // The final %n term captures the offset in the input string, which is used
    // to determine the path name. It *does not* increment the return value.
    // Refer to man 3 sscanf for details.
    if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4c %llx %hhx:%hhx %ld %n",
               &region.start, &region.end, permissions, &region.offset,
               &dev_major, &dev_minor, &inode, &path_index) < 7) {
      return false;
    }

    region.permissions = 0;

    if (permissions[0] == 'r')
      region.permissions |= MappedMemoryRegion::READ;
    else if (permissions[0] != '-')
      return false;

    if (permissions[1] == 'w')
      region.permissions |= MappedMemoryRegion::WRITE;
    else if (permissions[1] != '-')
      return false;

    if (permissions[2] == 'x')
      region.permissions |= MappedMemoryRegion::EXECUTE;
    else if (permissions[2] != '-')
      return false;

    if (permissions[3] == 'p')
      region.permissions |= MappedMemoryRegion::PRIVATE;
    else if (permissions[3] != 's' && permissions[3] != 'S')  // Shared memory.
      return false;

    // Pushing then assigning saves us a string copy.
    regions.push_back(region);
    regions.back().path.assign(line + path_index);
  }

  regions_out->swap(regions);
  return true;
}

}  // namespace debug
}  // namespace base
