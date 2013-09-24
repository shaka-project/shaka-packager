// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/condition_variable.h"

#include <errno.h>
#include <sys/time.h>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"

namespace base {

ConditionVariable::ConditionVariable(Lock* user_lock)
    : user_mutex_(user_lock->lock_.os_lock())
#if !defined(NDEBUG)
    , user_lock_(user_lock)
#endif
{
  int rv = pthread_cond_init(&condition_, NULL);
  DCHECK_EQ(0, rv);
}

ConditionVariable::~ConditionVariable() {
  int rv = pthread_cond_destroy(&condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Wait() {
  base::ThreadRestrictions::AssertWaitAllowed();
#if !defined(NDEBUG)
  user_lock_->CheckHeldAndUnmark();
#endif
  int rv = pthread_cond_wait(&condition_, user_mutex_);
  DCHECK_EQ(0, rv);
#if !defined(NDEBUG)
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::TimedWait(const TimeDelta& max_time) {
  base::ThreadRestrictions::AssertWaitAllowed();
  int64 usecs = max_time.InMicroseconds();

  // The timeout argument to pthread_cond_timedwait is in absolute time.
  struct timeval now;
  gettimeofday(&now, NULL);

  struct timespec abstime;
  abstime.tv_sec = now.tv_sec + (usecs / Time::kMicrosecondsPerSecond);
  abstime.tv_nsec = (now.tv_usec + (usecs % Time::kMicrosecondsPerSecond)) *
                    Time::kNanosecondsPerMicrosecond;
  abstime.tv_sec += abstime.tv_nsec / Time::kNanosecondsPerSecond;
  abstime.tv_nsec %= Time::kNanosecondsPerSecond;
  DCHECK_GE(abstime.tv_sec, now.tv_sec);  // Overflow paranoia

#if !defined(NDEBUG)
  user_lock_->CheckHeldAndUnmark();
#endif
  int rv = pthread_cond_timedwait(&condition_, user_mutex_, &abstime);
  DCHECK(rv == 0 || rv == ETIMEDOUT);
#if !defined(NDEBUG)
  user_lock_->CheckUnheldAndMark();
#endif
}

void ConditionVariable::Broadcast() {
  int rv = pthread_cond_broadcast(&condition_);
  DCHECK_EQ(0, rv);
}

void ConditionVariable::Signal() {
  int rv = pthread_cond_signal(&condition_);
  DCHECK_EQ(0, rv);
}

}  // namespace base
