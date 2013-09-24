// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/sys_utils.h"

#include "base/sys_info.h"
#include "jni/SysUtils_jni.h"

const int64 kLowEndMemoryThreshold = 1024 * 1024 * 512; // 512 mb.

// Defined and called by JNI
static jboolean IsLowEndDevice(JNIEnv* env, jclass clazz) {
  return base::android::SysUtils::IsLowEndDevice();
}

namespace base {
namespace android {

bool SysUtils::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool SysUtils::IsLowEndDevice() {
  return SysInfo::AmountOfPhysicalMemory() <= kLowEndMemoryThreshold;
}

SysUtils::SysUtils() { }

}  // namespace android
}  // namespace base
