// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/threading/thread_restrictions.h"

namespace base {

static const char* kLinuxStandardBaseVersionKeys[] = {
  "CHROMEOS_RELEASE_VERSION",
  "GOOGLE_RELEASE",
  "DISTRIB_RELEASE",
  NULL
};

const char kLinuxStandardBaseReleaseFile[] = "/etc/lsb-release";

struct ChromeOSVersionNumbers {
  ChromeOSVersionNumbers()
      : major_version(0),
        minor_version(0),
        bugfix_version(0),
        parsed(false) {
  }

  int32 major_version;
  int32 minor_version;
  int32 bugfix_version;
  bool parsed;
};

static LazyInstance<ChromeOSVersionNumbers>
    g_chrome_os_version_numbers = LAZY_INSTANCE_INITIALIZER;

// static
void SysInfo::OperatingSystemVersionNumbers(int32* major_version,
                                            int32* minor_version,
                                            int32* bugfix_version) {
  if (!g_chrome_os_version_numbers.Get().parsed) {
    // The other implementations of SysInfo don't block on the disk.
    // See http://code.google.com/p/chromium/issues/detail?id=60394
    // Perhaps the caller ought to cache this?
    // Temporary allowing while we work the bug out.
    ThreadRestrictions::ScopedAllowIO allow_io;

    FilePath path(kLinuxStandardBaseReleaseFile);
    std::string contents;
    if (file_util::ReadFileToString(path, &contents)) {
      g_chrome_os_version_numbers.Get().parsed = true;
      ParseLsbRelease(contents,
          &(g_chrome_os_version_numbers.Get().major_version),
          &(g_chrome_os_version_numbers.Get().minor_version),
          &(g_chrome_os_version_numbers.Get().bugfix_version));
    }
  }
  *major_version = g_chrome_os_version_numbers.Get().major_version;
  *minor_version = g_chrome_os_version_numbers.Get().minor_version;
  *bugfix_version = g_chrome_os_version_numbers.Get().bugfix_version;
}

// static
std::string SysInfo::GetLinuxStandardBaseVersionKey() {
  return std::string(kLinuxStandardBaseVersionKeys[0]);
}

// static
void SysInfo::ParseLsbRelease(const std::string& lsb_release,
                              int32* major_version,
                              int32* minor_version,
                              int32* bugfix_version) {
  size_t version_key_index = std::string::npos;
  for (int i = 0; kLinuxStandardBaseVersionKeys[i] != NULL; ++i) {
    version_key_index = lsb_release.find(kLinuxStandardBaseVersionKeys[i]);
    if (std::string::npos != version_key_index) {
      break;
    }
  }
  if (std::string::npos == version_key_index) {
    return;
  }

  size_t start_index = lsb_release.find_first_of('=', version_key_index);
  start_index++;  // Move past '='.
  size_t length = lsb_release.find_first_of('\n', start_index) - start_index;
  std::string version = lsb_release.substr(start_index, length);
  StringTokenizer tokenizer(version, ".");
  for (int i = 0; i < 3 && tokenizer.GetNext(); ++i) {
    if (0 == i) {
      StringToInt(StringPiece(tokenizer.token_begin(),
                              tokenizer.token_end()),
                  major_version);
      *minor_version = *bugfix_version = 0;
    } else if (1 == i) {
      StringToInt(StringPiece(tokenizer.token_begin(),
                              tokenizer.token_end()),
                  minor_version);
    } else {  // 2 == i
      StringToInt(StringPiece(tokenizer.token_begin(),
                              tokenizer.token_end()),
                  bugfix_version);
    }
  }
}

// static
FilePath SysInfo::GetLsbReleaseFilePath() {
  return FilePath(kLinuxStandardBaseReleaseFile);
}

}  // namespace base
