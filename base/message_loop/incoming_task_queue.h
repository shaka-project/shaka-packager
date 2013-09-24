// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_
#define BASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/pending_task.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace base {

class MessageLoop;
class WaitableEvent;

namespace internal {

// Implements a queue of tasks posted to the message loop running on the current
// thread. This class takes care of synchronizing posting tasks from different
// threads and together with MessageLoop ensures clean shutdown.
class BASE_EXPORT IncomingTaskQueue
    : public RefCountedThreadSafe<IncomingTaskQueue> {
 public:
  explicit IncomingTaskQueue(MessageLoop* message_loop);

  // Appends a task to the incoming queue. Posting of all tasks is routed though
  // AddToIncomingQueue() or TryAddToIncomingQueue() to make sure that posting
  // task is properly synchronized between different threads.
  //
  // Returns true if the task was successfully added to the queue, otherwise
  // returns false. In all cases, the ownership of |task| is transferred to the
  // called method.
  bool AddToIncomingQueue(const tracked_objects::Location& from_here,
                          const Closure& task,
                          TimeDelta delay,
                          bool nestable);

  // Same as AddToIncomingQueue() except that it will avoid blocking if the lock
  // is already held, and will in that case (when the lock is contended) fail to
  // add the task, and will return false.
  bool TryAddToIncomingQueue(const tracked_objects::Location& from_here,
                             const Closure& task);

  // Returns true if the message loop has high resolution timers enabled.
  // Provided for testing.
  bool IsHighResolutionTimerEnabledForTesting();

  // Returns true if the message loop is "idle". Provided for testing.
  bool IsIdleForTesting();

  // Takes the incoming queue lock, signals |caller_wait| and waits until
  // |caller_signal| is signalled.
  void LockWaitUnLockForTesting(WaitableEvent* caller_wait,
                                WaitableEvent* caller_signal);

  // Loads tasks from the |incoming_queue_| into |*work_queue|. Must be called
  // from the thread that is running the loop.
  void ReloadWorkQueue(TaskQueue* work_queue);

  // Disconnects |this| from the parent message loop.
  void WillDestroyCurrentMessageLoop();

 private:
  friend class RefCountedThreadSafe<IncomingTaskQueue>;
  virtual ~IncomingTaskQueue();

  // Calculates the time at which a PendingTask should run.
  TimeTicks CalculateDelayedRuntime(TimeDelta delay);

  // Adds a task to |incoming_queue_|. The caller retains ownership of
  // |pending_task|, but this function will reset the value of
  // |pending_task->task|. This is needed to ensure that the posting call stack
  // does not retain |pending_task->task| beyond this function call.
  bool PostPendingTask(PendingTask* pending_task);

#if defined(OS_WIN)
  TimeTicks high_resolution_timer_expiration_;
#endif

  // The lock that protects access to |incoming_queue_|, |message_loop_| and
  // |next_sequence_num_|.
  base::Lock incoming_queue_lock_;

  // An incoming queue of tasks that are acquired under a mutex for processing
  // on this instance's thread. These tasks have not yet been been pushed to
  // |message_loop_|.
  TaskQueue incoming_queue_;

  // Points to the message loop that owns |this|.
  MessageLoop* message_loop_;

  // The next sequence number to use for delayed tasks.
  int next_sequence_num_;

  DISALLOW_COPY_AND_ASSIGN(IncomingTaskQueue);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_
