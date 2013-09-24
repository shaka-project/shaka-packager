// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_mach_port.h"

namespace base {
namespace mac {

ScopedMachPort::ScopedMachPort(mach_port_t port) : port_(port) {
}

ScopedMachPort::~ScopedMachPort() {
  reset();
}

void ScopedMachPort::reset(mach_port_t port) {
  if (port_ != MACH_PORT_NULL) {
    mach_port_deallocate(mach_task_self(), port_);
  }
  port_ = port;
}

}  // namespace mac
}  // namespace base
