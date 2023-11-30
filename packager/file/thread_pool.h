// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_THREAD_POOL_H_
#define PACKAGER_FILE_THREAD_POOL_H_

#include <functional>
#include <queue>

#include <absl/base/thread_annotations.h>
#include <absl/synchronization/mutex.h>

#include <packager/macros/classes.h>

namespace shaka {

/// A simple thread pool.  We used to get this from Chromium base::, but there
/// is no replacement in the C++ standard library nor in absl.
/// (As of June 2022.)  The pool will grow when there are no threads available
/// to handle a task, and it will shrink when a thread is idle for too long.
class ThreadPool {
 public:
  typedef std::function<void()> Task;

  ThreadPool();
  ~ThreadPool();

  /// Find or spawn a worker thread to handle |task|.
  /// @param task A potentially long-running task to be handled by the pool.
  void PostTask(const Task& task);

  static ThreadPool instance;

 private:
  /// Stop handing out tasks to workers, wake up all threads, and make them
  /// exit.
  void Terminate();

  Task WaitForTask();
  void ThreadMain();

  absl::Mutex mutex_;
  absl::CondVar tasks_available_ ABSL_GUARDED_BY(mutex_);
  std::queue<Task> tasks_ ABSL_GUARDED_BY(mutex_);
  size_t num_idle_threads_ ABSL_GUARDED_BY(mutex_);
  bool terminated_ ABSL_GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

}  // namespace shaka

#endif  // PACKAGER_FILE_THREAD_POOL_H_
