// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"

#include "base/android/activity_status.h"
#include "base/android/build_info.h"
#include "base/android/cpu_features.h"
#include "base/android/important_file_writer_android.h"
#include "base/android/java_handler_thread.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/android/path_service_android.h"
#include "base/android/path_utils.h"
#include "base/android/sys_utils.h"
#include "base/android/thread_utils.h"
#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_pump_android.h"
#include "base/power_monitor/power_monitor_device_source_android.h"

#if defined(GOOGLE_TV)
#include "base/android/context_types.h"
#endif

namespace base {
namespace android {

static RegistrationMethod kBaseRegisteredMethods[] = {
  { "ActivityStatus", base::android::ActivityStatus::RegisterBindings },
  { "BuildInfo", base::android::BuildInfo::RegisterBindings },
#if defined(GOOGLE_TV)
  { "ContextTypes", base::android::RegisterContextTypes },
#endif
  { "CpuFeatures", base::android::RegisterCpuFeatures },
  { "ImportantFileWriterAndroid",
    base::android::RegisterImportantFileWriterAndroid },
  { "MemoryPressureListenerAndroid",
      base::android::MemoryPressureListenerAndroid::Register },
  { "JavaHandlerThread", base::android::JavaHandlerThread::RegisterBindings },
  { "PathService", base::android::RegisterPathService },
  { "PathUtils", base::android::RegisterPathUtils },
  { "SystemMessageHandler", base::MessagePumpForUI::RegisterBindings },
  { "SysUtils", base::android::SysUtils::Register },
  { "PowerMonitor", base::RegisterPowerMonitor },
  { "ThreadUtils", base::RegisterThreadUtils },
};

bool RegisterJni(JNIEnv* env) {
  TRACE_EVENT0("startup", "base_android::RegisterJni");
  return RegisterNativeMethods(env, kBaseRegisteredMethods,
                               arraysize(kBaseRegisteredMethods));
}

}  // namespace android
}  // namespace base
