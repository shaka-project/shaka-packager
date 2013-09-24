// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/incoming_task_queue.h"

#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"

namespace base {
namespace internal {

IncomingTaskQueue::IncomingTaskQueue(MessageLoop* message_loop)
    : message_loop_(message_loop),
      next_sequence_num_(0) {
}

bool IncomingTaskQueue::AddToIncomingQueue(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay,
    bool nestable) {
  AutoLock locked(incoming_queue_lock_);
  PendingTask pending_task(
      from_here, task, CalculateDelayedRuntime(delay), nestable);
  return PostPendingTask(&pending_task);
}

bool IncomingTaskQueue::TryAddToIncomingQueue(
    const tracked_objects::Location& from_here,
    const Closure& task) {
  if (!incoming_queue_lock_.Try()) {
    // Reset |task|.
    Closure local_task = task;
    return false;
  }

  AutoLock locked(incoming_queue_lock_, AutoLock::AlreadyAcquired());
  PendingTask pending_task(
      from_here, task, CalculateDelayedRuntime(TimeDelta()), true);
  return PostPendingTask(&pending_task);
}

bool IncomingTaskQueue::IsHighResolutionTimerEnabledForTesting() {
#if defined(OS_WIN)
  return !high_resolution_timer_expiration_.is_null();
#else
  return true;
#endif
}

bool IncomingTaskQueue::IsIdleForTesting() {
  AutoLock lock(incoming_queue_lock_);
  return incoming_queue_.empty();
}

void IncomingTaskQueue::LockWaitUnLockForTesting(WaitableEvent* caller_wait,
                                                 WaitableEvent* caller_signal) {
  AutoLock lock(incoming_queue_lock_);
  caller_wait->Signal();
  caller_signal->Wait();
}

void IncomingTaskQueue::ReloadWorkQueue(TaskQueue* work_queue) {
  // Make sure no tasks are lost.
  DCHECK(work_queue->empty());

  // Acquire all we can from the inter-thread queue with one lock acquisition.
  AutoLock lock(incoming_queue_lock_);
  if (!incoming_queue_.empty())
    incoming_queue_.Swap(work_queue);  // Constant time

  DCHECK(incoming_queue_.empty());
}

void IncomingTaskQueue::WillDestroyCurrentMessageLoop() {
#if defined(OS_WIN)
  // If we left the high-resolution timer activated, deactivate it now.
  // Doing this is not-critical, it is mainly to make sure we track
  // the high resolution timer activations properly in our unit tests.
  if (!high_resolution_timer_expiration_.is_null()) {
    Time::ActivateHighResolutionTimer(false);
    high_resolution_timer_expiration_ = TimeTicks();
  }
#endif

  AutoLock lock(incoming_queue_lock_);
  message_loop_ = NULL;
}

IncomingTaskQueue::~IncomingTaskQueue() {
  // Verify that WillDestroyCurrentMessageLoop() has been called.
  DCHECK(!message_loop_);
}

TimeTicks IncomingTaskQueue::CalculateDelayedRuntime(TimeDelta delay) {
  TimeTicks delayed_run_time;
  if (delay > TimeDelta()) {
    delayed_run_time = TimeTicks::Now() + delay;

#if defined(OS_WIN)
    if (high_resolution_timer_expiration_.is_null()) {
      // Windows timers are granular to 15.6ms.  If we only set high-res
      // timers for those under 15.6ms, then a 18ms timer ticks at ~32ms,
      // which as a percentage is pretty inaccurate.  So enable high
      // res timers for any timer which is within 2x of the granularity.
      // This is a tradeoff between accuracy and power management.
      bool needs_high_res_timers = delay.InMilliseconds() <
          (2 * Time::kMinLowResolutionThresholdMs);
      if (needs_high_res_timers) {
        if (Time::ActivateHighResolutionTimer(true)) {
          high_resolution_timer_expiration_ = TimeTicks::Now() +
              TimeDelta::FromMilliseconds(
                  MessageLoop::kHighResolutionTimerModeLeaseTimeMs);
        }
      }
    }
#endif
  } else {
    DCHECK_EQ(delay.InMilliseconds(), 0) << "delay should not be negative";
  }

#if defined(OS_WIN)
  if (!high_resolution_timer_expiration_.is_null()) {
    if (TimeTicks::Now() > high_resolution_timer_expiration_) {
      Time::ActivateHighResolutionTimer(false);
      high_resolution_timer_expiration_ = TimeTicks();
    }
  }
#endif

  return delayed_run_time;
}

bool IncomingTaskQueue::PostPendingTask(PendingTask* pending_task) {
  // Warning: Don't try to short-circuit, and handle this thread's tasks more
  // directly, as it could starve handling of foreign threads.  Put every task
  // into this queue.

  // This should only be called while the lock is taken.
  incoming_queue_lock_.AssertAcquired();

  if (!message_loop_) {
    pending_task->task.Reset();
    return false;
  }

  // Initialize the sequence number. The sequence number is used for delayed
  // tasks (to faciliate FIFO sorting when two tasks have the same
  // delayed_run_time value) and for identifying the task in about:tracing.
  pending_task->sequence_num = next_sequence_num_++;

  TRACE_EVENT_FLOW_BEGIN0("task", "MessageLoop::PostTask",
      TRACE_ID_MANGLE(message_loop_->GetTaskTraceID(*pending_task)));

  bool was_empty = incoming_queue_.empty();
  incoming_queue_.push(*pending_task);
  pending_task->task.Reset();

  // Wake up the pump.
  message_loop_->ScheduleWork(was_empty);

  return true;
}

}  // namespace internal
}  // namespace base
