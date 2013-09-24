// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_SCOPED_MACH_PORT_H_
#define BASE_MAC_SCOPED_MACH_PORT_H_

#include <mach/mach.h>

#include "base/basictypes.h"
#include "base/base_export.h"

namespace base {
namespace mac {

// A class for managing the life of a Mach port, releasing via
// mach_port_deallocate either its send and/or receive rights.
class BASE_EXPORT ScopedMachPort {
 public:
  // Creates a scoper by taking ownership of the port.
  explicit ScopedMachPort(mach_port_t port);

  ~ScopedMachPort();

  void reset(mach_port_t port = MACH_PORT_NULL);

  operator mach_port_t() const {
    return port_;
  }

  mach_port_t get() const {
    return port_;
  }

 private:
  mach_port_t port_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMachPort);
};

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_SCOPED_MACH_PORT_H_
