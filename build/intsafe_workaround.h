// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_INTSAFE_WORKAROUND_H_
#define BUILD_INTSAFE_WORKAROUND_H_

// Workaround for:
// http://connect.microsoft.com/VisualStudio/feedback/details/621653/
// http://crbug.com/225822
// Note that we can't actually include <stdint.h> here because there's other
// code in third_party that has partial versions of stdint types that conflict.
#include <intsafe.h>
#undef INT8_MIN
#undef INT16_MIN
#undef INT32_MIN
#undef INT64_MIN
#undef INT8_MAX
#undef UINT8_MAX
#undef INT16_MAX
#undef UINT16_MAX
#undef INT32_MAX
#undef UINT32_MAX
#undef INT64_MAX
#undef UINT64_MAX

#endif  // BUILD_INTSAFE_WORKAROUND_H_
