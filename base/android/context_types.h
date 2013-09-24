// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_CONTEXT_TYPES_H_
#define BASE_ANDROID_CONTEXT_TYPES_H_

#include <jni.h>

#include "base/base_export.h"

namespace base {
namespace android {

BASE_EXPORT bool IsRunningInWebapp();

bool RegisterContextTypes(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_CONTEXT_TYPES_H_
