// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/context_types.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "jni/ContextTypes_jni.h"

namespace base {
namespace android {

bool IsRunningInWebapp() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<bool>(
     Java_ContextTypes_isRunningInWebapp(env, GetApplicationContext()));
}

bool RegisterContextTypes(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace base
