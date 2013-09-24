// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_info.h"

#include <windows.h>

#include "base/basictypes.h"
#include "base/time/time.h"

namespace base {

//static
const Time CurrentProcessInfo::CreationTime() {
  FILETIME creation_time = {};
  FILETIME ignore = {};
  if (::GetProcessTimes(::GetCurrentProcess(), &creation_time, &ignore,
      &ignore, &ignore) == false)
    return Time();

  return Time::FromFileTime(creation_time);
}

}  // namespace base
