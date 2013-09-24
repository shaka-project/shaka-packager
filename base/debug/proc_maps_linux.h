// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_PROC_MAPS_LINUX_H_
#define BASE_DEBUG_PROC_MAPS_LINUX_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"

namespace base {
namespace debug {

// Describes a region of mapped memory and the path of the file mapped.
struct MappedMemoryRegion {
  enum Permission {
    READ = 1 << 0,
    WRITE = 1 << 1,
    EXECUTE = 1 << 2,
    PRIVATE = 1 << 3,  // If set, region is private, otherwise it is shared.
  };

  // The address range [start,end) of mapped memory.
  uintptr_t start;
  uintptr_t end;

  // Byte offset into |path| of the range mapped into memory.
  unsigned long long offset;

  // Bitmask of read/write/execute/private/shared permissions.
  uint8 permissions;

  // Name of the file mapped into memory.
  //
  // NOTE: path names aren't guaranteed to point at valid files. For example,
  // "[heap]" and "[stack]" are used to represent the location of the process'
  // heap and stack, respectively.
  std::string path;
};

// Reads the data from /proc/self/maps and stores the result in |proc_maps|.
// Returns true if successful, false otherwise.
BASE_EXPORT bool ReadProcMaps(std::string* proc_maps);

// Parses /proc/<pid>/maps input data and stores in |regions|. Returns true
// and updates |regions| if and only if all of |input| was successfully parsed.
BASE_EXPORT bool ParseProcMaps(const std::string& input,
                               std::vector<MappedMemoryRegion>* regions);

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_PROC_MAPS_LINUX_H_
