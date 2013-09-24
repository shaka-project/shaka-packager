// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <float.h>

#include "base/safe_numerics.h"

using base::internal::IsValidNumericCast;

#if defined(NCTEST_NO_FLOATING_POINT_1)  // [r"size of array is negative"]

void WontCompile() {
  IsValidNumericCast<float>(0.0);
}

#elif defined(NCTEST_NO_FLOATING_POINT_2)  // [r"size of array is negative"]

void WontCompile() {
  IsValidNumericCast<double>(0.0f);
}

#elif defined(NCTEST_NO_FLOATING_POINT_3)  // [r"size of array is negative"]

void WontCompile() {
  IsValidNumericCast<int>(DBL_MAX);
}

#endif
