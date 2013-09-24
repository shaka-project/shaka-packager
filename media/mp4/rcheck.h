// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_RCHECK_H_
#define MEDIA_MP4_RCHECK_H_

#include "base/logging.h"

#define RCHECK(x) \
    do { \
      if (!(x)) { \
        DLOG(ERROR) << "Failure while parsing MP4: " << #x; \
        return false; \
      } \
    } while (0)

#endif  // MEDIA_MP4_RCHECK_H_
