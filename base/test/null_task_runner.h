// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"

namespace base {

// Helper class for tests that need to provide an implementation of a
// *TaskRunner class but don't actually care about tasks being run.

class NullTaskRunner : public base::SingleThreadTaskRunner {
 public:
  NullTaskRunner();

  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;
  // Always returns true to avoid triggering DCHECKs.
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;

 protected:
  virtual ~NullTaskRunner();

  DISALLOW_COPY_AND_ASSIGN(NullTaskRunner);
};

}  // namespace
