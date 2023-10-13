// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MACROS_STATUS_H_
#define PACKAGER_MACROS_STATUS_H_

#include <packager/status.h>

// Evaluates an expression that produces a `Status`. If the status is not
// ok, returns it from the current function.
#define RETURN_IF_ERROR(expr)                                                \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    shaka::Status _status = (expr);                                          \
    if (!_status.ok())                                                       \
      return _status;                                                        \
  } while (0)

// TODO(kqyang): Support build Status and update Status message through "<<".

#endif  // PACKAGER_MACROS_STATUS_H_
