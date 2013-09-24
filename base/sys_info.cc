// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include "base/time/time.h"

namespace base {

// static
int64 SysInfo::Uptime() {
  // This code relies on an implementation detail of TimeTicks::Now() - that
  // its return value happens to coincide with the system uptime value in
  // microseconds, on Win/Mac/iOS/Linux/ChromeOS and Android.
  int64 uptime_in_microseconds = TimeTicks::Now().ToInternalValue();
  return uptime_in_microseconds / 1000;
}

}  // namespace base
