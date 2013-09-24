// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/scheduler.h"

#include "base/bind.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/standard_out.h"

Scheduler* g_scheduler = NULL;

Scheduler::Scheduler()
    : pool_(new base::SequencedWorkerPool(32, "worker_")),
      input_file_manager_(new InputFileManager),
      verbose_logging_(false),
      work_count_(0),
      is_failed_(false) {
  g_scheduler = this;
}

Scheduler::~Scheduler() {
  g_scheduler = NULL;
}

bool Scheduler::Run() {
  runner_.Run();
  pool_->Shutdown();
  return !is_failed();
}

void Scheduler::Log(const std::string& verb, const std::string& msg) {
  if (base::MessageLoop::current() == &main_loop_) {
    LogOnMainThread(verb, msg);
  } else {
    // The run loop always joins on the sub threads, so the lifetime of this
    // object outlives the invocations of this function, hence "unretained".
    main_loop_.PostTask(FROM_HERE,
                        base::Bind(&Scheduler::LogOnMainThread,
                                   base::Unretained(this), verb, msg));
  }
}

void Scheduler::FailWithError(const Err& err) {
  DCHECK(err.has_error());
  {
    base::AutoLock lock(lock_);

    if (is_failed_)
      return;  // Ignore errors once we see one.
    is_failed_ = true;
  }

  if (base::MessageLoop::current() == &main_loop_) {
    FailWithErrorOnMainThread(err);
  } else {
    // The run loop always joins on the sub threads, so the lifetime of this
    // object outlives the invocations of this function, hence "unretained".
    main_loop_.PostTask(FROM_HERE,
                        base::Bind(&Scheduler::FailWithErrorOnMainThread,
                                   base::Unretained(this), err));
  }
}

void Scheduler::ScheduleWork(const base::Closure& work) {
  IncrementWorkCount();
  pool_->PostWorkerTaskWithShutdownBehavior(
      FROM_HERE, base::Bind(&Scheduler::DoWork,
                            base::Unretained(this), work),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

void Scheduler::ScheduleTargetFileWrite(const Target* target) {
  pool_->PostWorkerTaskWithShutdownBehavior(
      FROM_HERE, base::Bind(&Scheduler::DoTargetFileWrite,
                            base::Unretained(this), target),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

void Scheduler::AddGenDependency(const base::FilePath& file) {
  base::AutoLock lock(lock_);
  gen_dependencies_.push_back(file);
}

std::vector<base::FilePath> Scheduler::GetGenDependencies() const {
  base::AutoLock lock(lock_);
  return gen_dependencies_;
}

void Scheduler::IncrementWorkCount() {
  base::AtomicRefCountInc(&work_count_);
}

void Scheduler::DecrementWorkCount() {
  if (!base::AtomicRefCountDec(&work_count_)) {
    if (base::MessageLoop::current() == &main_loop_) {
      OnComplete();
    } else {
      main_loop_.PostTask(FROM_HERE,
                          base::Bind(&Scheduler::OnComplete,
                                     base::Unretained(this)));
    }
  }
}

void Scheduler::LogOnMainThread(const std::string& verb,
                                const std::string& msg) {
  OutputString(verb, DECORATION_YELLOW);
  OutputString(" " + msg + "\n");
}

void Scheduler::FailWithErrorOnMainThread(const Err& err) {
  err.PrintToStdout();
  runner_.Quit();
}

void Scheduler::DoTargetFileWrite(const Target* target) {
  NinjaTargetWriter::RunAndWriteFile(target);
}

void Scheduler::DoWork(const base::Closure& closure) {
  closure.Run();
  DecrementWorkCount();
}

void Scheduler::OnComplete() {
  // Should be called on the main thread.
  DCHECK(base::MessageLoop::current() == main_loop());
  runner_.Quit();
}
