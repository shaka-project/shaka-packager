// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/thread_pool.h>

#include <thread>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/time/time.h>

namespace shaka {

namespace {

const absl::Duration kMaxThreadIdleTime = absl::Minutes(10);

}  // namespace

// static
ThreadPool ThreadPool::instance;

ThreadPool::ThreadPool() : num_idle_threads_(0), terminated_(false) {}

ThreadPool::~ThreadPool() {
  Terminate();
}

void ThreadPool::PostTask(const std::function<void()>& task) {
  absl::MutexLock lock(&mutex_);

  DCHECK(!terminated_) << "Should not call PostTask after Terminate!";

  if (terminated_) {
    return;
  }

  // An empty task is used internally to signal the thread to terminate.  This
  // should never be sent on input.
  if (!task) {
    DLOG(ERROR) << "Should not post an empty task!";
    return;
  }

  tasks_.push(std::move(task));

  if (num_idle_threads_ >= tasks_.size()) {
    // We have enough threads available.
    tasks_available_.SignalAll();
  } else {
    // We need to start an additional thread.
    std::thread thread(std::bind(&ThreadPool::ThreadMain, this));
    thread.detach();
  }
}

void ThreadPool::Terminate() {
  {
    absl::MutexLock lock(&mutex_);
    terminated_ = true;
    while (!tasks_.empty()) {
      tasks_.pop();
    }
  }
  tasks_available_.SignalAll();
}

ThreadPool::Task ThreadPool::WaitForTask() {
  absl::MutexLock lock(&mutex_);
  if (terminated_) {
    // The pool is terminated.  Terminate this thread.
    return Task();
  }

  if (tasks_.empty()) {
    num_idle_threads_++;
    // Wait for a task, up to the maximum idle time.
    tasks_available_.WaitWithTimeout(&mutex_, kMaxThreadIdleTime);
    num_idle_threads_--;

    if (tasks_.empty()) {
      // No work before the timeout.  Terminate this thread.
      return Task();
    }
  }

  // Get the next task from the queue.
  Task task = tasks_.front();
  tasks_.pop();
  return task;
}

void ThreadPool::ThreadMain() {
  while (true) {
    auto task = WaitForTask();
    if (!task) {
      // An empty task signals the thread to terminate.
      return;
    }

    // Run the task, then loop to wait for another.
    task();
  }
}

}  // namespace shaka
