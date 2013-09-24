// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <sched.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/safe_strerror_posix.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_restrictions.h"
#include "base/tracked_objects.h"

#if !defined(OS_NACL)
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace base {

namespace {
int ThreadNiceValue(ThreadPriority priority) {
  static const int threadPriorityAudio = -10;
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

// static
void PlatformThread::SetName(const char* name) {
  ThreadIdNameManager::GetInstance()->SetName(CurrentId(), name);
  tracked_objects::ThreadData::InitializeThreadContext(name);

#ifndef OS_NACL
  // On linux we can get the thread names to show up in the debugger by setting
  // the process name for the LWP.  We don't want to do this for the main
  // thread because that would rename the process, causing tools like killall
  // to stop working.
  if (PlatformThread::CurrentId() == getpid())
    return;

  // http://0pointer.de/blog/projects/name-your-threads.html
  // Set the name for the LWP (which gets truncated to 15 characters).
  // Note that glibc also has a 'pthread_setname_np' api, but it may not be
  // available everywhere and it's only benefit over using prctl directly is
  // that it can set the name of threads other than the current thread.
  int err = prctl(PR_SET_NAME, name);
  // We expect EPERM failures in sandboxed processes, just ignore those.
  if (err < 0 && errno != EPERM)
    DPLOG(ERROR) << "prctl(PR_SET_NAME)";
#endif
}

// static
void PlatformThread::SetThreadPriority(PlatformThreadHandle handle,
                                       ThreadPriority priority) {
#if !defined(OS_NACL)
  if (priority == kThreadPriority_RealtimeAudio) {
    const int kRealTimePrio = 8;

    struct sched_param sched_param;
    memset(&sched_param, 0, sizeof(sched_param));
    sched_param.sched_priority = kRealTimePrio;

    if (pthread_setschedparam(pthread_self(), SCHED_RR, &sched_param) == 0) {
      // Got real time priority, no need to set nice level.
      return;
    }
  }

  // setpriority(2) will set a thread's priority if it is passed a tid as
  // the 'process identifier', not affecting the rest of the threads in the
  // process. Setting this priority will only succeed if the user has been
  // granted permission to adjust nice values on the system.
  DCHECK_NE(handle.id_, kInvalidThreadId);
  int kNiceSetting = ThreadNiceValue(priority);
  if (setpriority(PRIO_PROCESS, handle.id_, kNiceSetting))
    LOG(ERROR) << "Failed to set nice value of thread to " << kNiceSetting;
#endif  // !OS_NACL
}

void InitThreading() {
}

void InitOnThread() {
}

void TerminateOnThread() {
}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
  return 0;
}

}  // namespace base
