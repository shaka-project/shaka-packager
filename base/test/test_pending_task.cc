// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_pending_task.h"

namespace base {

TestPendingTask::TestPendingTask() : nestability(NESTABLE) {}

TestPendingTask::TestPendingTask(
    const tracked_objects::Location& location,
    const Closure& task,
    TimeTicks post_time,
    TimeDelta delay,
    TestNestability nestability)
    : location(location),
      task(task),
      post_time(post_time),
      delay(delay),
      nestability(nestability) {}

TimeTicks TestPendingTask::GetTimeToRun() const {
  return post_time + delay;
}

bool TestPendingTask::ShouldRunBefore(const TestPendingTask& other) const {
  if (nestability != other.nestability)
    return (nestability == NESTABLE);
  return GetTimeToRun() < other.GetTimeToRun();
}

TestPendingTask::~TestPendingTask() {}

}  // namespace base
