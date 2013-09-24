// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <sys/system_properties.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"

namespace {

// Default version of Android to fall back to when actual version numbers
// cannot be acquired.
// TODO(dfalcantara): Keep this reasonably up to date with the latest publicly
//                    available version of Android.
const int kDefaultAndroidMajorVersion = 4;
const int kDefaultAndroidMinorVersion = 3;
const int kDefaultAndroidBugfixVersion = 0;

// Parse out the OS version numbers from the system properties.
void ParseOSVersionNumbers(const char* os_version_str,
                           int32 *major_version,
                           int32 *minor_version,
                           int32 *bugfix_version) {
  if (os_version_str[0]) {
    // Try to parse out the version numbers from the string.
    int num_read = sscanf(os_version_str, "%d.%d.%d", major_version,
                          minor_version, bugfix_version);

    if (num_read > 0) {
      // If we don't have a full set of version numbers, make the extras 0.
      if (num_read < 2) *minor_version = 0;
      if (num_read < 3) *bugfix_version = 0;
      return;
    }
  }

  // For some reason, we couldn't parse the version number string.
  *major_version = kDefaultAndroidMajorVersion;
  *minor_version = kDefaultAndroidMinorVersion;
  *bugfix_version = kDefaultAndroidBugfixVersion;
}

// Parses a system property (specified with unit 'k','m' or 'g').
// Returns a value in bytes.
// Returns -1 if the string could not be parsed.
int64 ParseSystemPropertyBytes(const base::StringPiece& str) {
  const int64 KB = 1024;
  const int64 MB = 1024 * KB;
  const int64 GB = 1024 * MB;
  if (str.size() == 0u)
    return -1;
  int64 unit_multiplier = 1;
  size_t length = str.size();
  if (str[length - 1] == 'k') {
    unit_multiplier = KB;
    length--;
  } else if (str[length - 1] == 'm') {
    unit_multiplier = MB;
    length--;
  } else if (str[length - 1] == 'g') {
    unit_multiplier = GB;
    length--;
  }
  int64 result = 0;
  bool parsed = base::StringToInt64(str.substr(0, length), &result);
  bool negative = result <= 0;
  bool overflow = result >= std::numeric_limits<int64>::max() / unit_multiplier;
  if (!parsed || negative || overflow)
    return -1;
  return result * unit_multiplier;
}

int GetDalvikHeapSizeMB() {
  char heap_size_str[PROP_VALUE_MAX];
  __system_property_get("dalvik.vm.heapsize", heap_size_str);
  // dalvik.vm.heapsize property is writable by a root user.
  // Clamp it to reasonable range as a sanity check,
  // a typical android device will never have less than 48MB.
  const int64 MB = 1024 * 1024;
  int64 result = ParseSystemPropertyBytes(heap_size_str);
  if (result == -1) {
     // We should consider not exposing these values if they are not reliable.
     LOG(ERROR) << "Can't parse dalvik.vm.heapsize: " << heap_size_str;
     result = base::SysInfo::AmountOfPhysicalMemoryMB() / 3;
  }
  result = std::min<int64>(std::max<int64>(32 * MB, result), 1024 * MB) / MB;
  return static_cast<int>(result);
}

int GetDalvikHeapGrowthLimitMB() {
  char heap_size_str[PROP_VALUE_MAX];
  __system_property_get("dalvik.vm.heapgrowthlimit", heap_size_str);
  // dalvik.vm.heapgrowthlimit property is writable by a root user.
  // Clamp it to reasonable range as a sanity check,
  // a typical android device will never have less than 24MB.
  const int64 MB = 1024 * 1024;
  int64 result = ParseSystemPropertyBytes(heap_size_str);
  if (result == -1) {
     // We should consider not exposing these values if they are not reliable.
     LOG(ERROR) << "Can't parse dalvik.vm.heapgrowthlimit: " << heap_size_str;
     result = base::SysInfo::AmountOfPhysicalMemoryMB() / 6;
  }
  result = std::min<int64>(std::max<int64>(16 * MB, result), 512 * MB) / MB;
  return static_cast<int>(result);
}

}  // anonymous namespace

namespace base {

std::string SysInfo::OperatingSystemName() {
  return "Android";
}

std::string SysInfo::GetAndroidBuildCodename() {
  char os_version_codename_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.version.codename", os_version_codename_str);
  return std::string(os_version_codename_str);
}

std::string SysInfo::GetAndroidBuildID() {
  char os_build_id_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.id", os_build_id_str);
  return std::string(os_build_id_str);
}

std::string SysInfo::GetDeviceName() {
  char device_model_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.model", device_model_str);
  return std::string(device_model_str);
}

std::string SysInfo::OperatingSystemVersion() {
  int32 major, minor, bugfix;
  OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  return StringPrintf("%d.%d.%d", major, minor, bugfix);
}

void SysInfo::OperatingSystemVersionNumbers(int32* major_version,
                                            int32* minor_version,
                                            int32* bugfix_version) {
  // Read the version number string out from the properties.
  char os_version_str[PROP_VALUE_MAX];
  __system_property_get("ro.build.version.release", os_version_str);

  // Parse out the numbers.
  ParseOSVersionNumbers(os_version_str, major_version, minor_version,
                        bugfix_version);
}

int SysInfo::DalvikHeapSizeMB() {
  static int heap_size = GetDalvikHeapSizeMB();
  return heap_size;
}

int SysInfo::DalvikHeapGrowthLimitMB() {
  static int heap_growth_limit = GetDalvikHeapGrowthLimitMB();
  return heap_growth_limit;
}


}  // namespace base
