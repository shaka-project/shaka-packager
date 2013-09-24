// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_FRAMEWORK_CHROME_IMPORTANT_FILE_WRITE_ANDROID_H_
#define NATIVE_FRAMEWORK_CHROME_IMPORTANT_FILE_WRITE_ANDROID_H_

#include <jni.h>

namespace base {
namespace android {

bool RegisterImportantFileWriterAndroid(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // NATIVE_FRAMEWORK_CHROME_IMPORTANT_FILE_WRITE_ANDROID_H_
