// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_BASE_RCHECK_H_
#define PACKAGER_MEDIA_BASE_RCHECK_H_

#include <absl/log/log.h>

#define RCHECK(x)                                       \
  do {                                                  \
    if (!(x)) {                                         \
      LOG(ERROR) << "Failure while processing: " << #x; \
      return false;                                     \
    }                                                   \
  } while (0)

#endif  // PACKAGER_MEDIA_BASE_RCHECK_H_
