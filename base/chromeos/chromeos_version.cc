// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/chromeos/chromeos_version.h"

#include <stdlib.h>
#include <string.h>

#include "base/logging.h"

namespace base {
namespace chromeos {

bool IsRunningOnChromeOS() {
  // Check if the user name is chronos. Note that we don't go with
  // getuid() + getpwuid_r() as it may end up reading /etc/passwd, which
  // can be expensive.
  const char* user = getenv("USER");
  return user && strcmp(user, "chronos") == 0;
}

}  // namespace chromeos
}  // namespace base
