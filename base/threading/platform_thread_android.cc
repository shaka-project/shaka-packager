// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <sys/resource.h>

#include "base/android/jni_android.h"
#include "base/android/thread_utils.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/tracked_objects.h"
#include "jni/ThreadUtils_jni.h"

namespace base {

namespace {
int ThreadNiceValue(ThreadPriority priority) {
  // These nice values are taken from Android, which uses nice
  // values like linux, but defines some preset nice values.
  //   Process.THREAD_PRIORITY_AUDIO = -16
  //   Process.THREAD_PRIORITY_BACKGROUND = 10
  //   Process.THREAD_PRIORITY_DEFAULT = 0;
  //   Process.THREAD_PRIORITY_DISPLAY = -4;
  //   Process.THREAD_PRIORITY_FOREGROUND = -2;
  //   Process.THREAD_PRIORITY_LESS_FAVORABLE = 1;
  //   Process.THREAD_PRIORITY_LOWEST = 19;
  //   Process.THREAD_PRIORITY_MORE_FAVORABLE = -1;
  //   Process.THREAD_PRIORITY_URGENT_AUDIO = -19;
  //   Process.THREAD_PRIORITY_URGENT_DISPLAY = -8;
  // We use -6 for display, but we may want to split this
  // into urgent (-8) and non-urgent (-4).
  static const int threadPriorityAudio = -16;
  static const int threadPriorityBackground = 10;
  static const int threadPriorityDefault = 0;
  static const int threadPriorityDisplay = -6;
  switch (priority) {
    case kThreadPriority_RealtimeAudio:
      return threadPriorityAudio;
    case kThreadPriority_Background:
      return threadPriorityBackground;
    case kThreadPriority_Normal:
      return threadPriorityDefault;
    case kThreadPriority_Display:
      return threadPriorityDisplay;
    default:
      NOTREACHED() << "Unknown priority.";
      return 0;
  }
}
} // namespace

//static
void PlatformThread::SetThreadPriority(PlatformThreadHandle handle,
                                       ThreadPriority priority) {
  // On Android, we set the Audio priority through JNI as Audio priority
  // will also allow the process to run while it is backgrounded.
  if (priority == kThreadPriority_RealtimeAudio) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ThreadUtils_setThreadPriorityAudio(env, PlatformThread::CurrentId());
    return;
  }

  // setpriority(2) will set a thread's priority if it is passed a tid as
  // the 'process identifier', not affecting the rest of the threads in the
  // process. Setting this priority will only succeed if the user has been
  // granted permission to adjust nice values on the system.
  DCHECK_NE(handle.id_, kInvalidThreadId);
  int kNiceSetting = ThreadNiceValue(priority);
  if (setpriority(PRIO_PROCESS, handle.id_, kNiceSetting))
    LOG(ERROR) << "Failed to set nice value of thread to " << kNiceSetting;
}

void PlatformThread::SetName(const char* name) {
  ThreadIdNameManager::GetInstance()->SetName(CurrentId(), name);
  tracked_objects::ThreadData::InitializeThreadContext(name);
}


void InitThreading() {
}

void InitOnThread() {
  // Threads on linux/android may inherit their priority from the thread
  // where they were created. This sets all new threads to the default.
  PlatformThread::SetThreadPriority(PlatformThread::CurrentHandle(),
                                    kThreadPriority_Normal);
}

void TerminateOnThread() {
  base::android::DetachFromVM();
}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
  return 0;
}

bool RegisterThreadUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace base
