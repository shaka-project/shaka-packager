// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"

#include "base/bind.h"

namespace base {

RunLoop::RunLoop()
    : loop_(MessageLoop::current()),
      weak_factory_(this),
      previous_run_loop_(NULL),
      run_depth_(0),
      run_called_(false),
      quit_called_(false),
      running_(false),
      quit_when_idle_received_(false) {
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
   dispatcher_ = NULL;
#endif
}

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
RunLoop::RunLoop(MessageLoop::Dispatcher* dispatcher)
    : loop_(MessageLoop::current()),
      weak_factory_(this),
      previous_run_loop_(NULL),
      dispatcher_(dispatcher),
      run_depth_(0),
      run_called_(false),
      quit_called_(false),
      running_(false),
      quit_when_idle_received_(false) {
}
#endif

RunLoop::~RunLoop() {
}

void RunLoop::Run() {
  if (!BeforeRun())
    return;
  loop_->RunHandler();
  AfterRun();
}

void RunLoop::RunUntilIdle() {
  quit_when_idle_received_ = true;
  Run();
}

void RunLoop::Quit() {
  quit_called_ = true;
  if (running_ && loop_->run_loop_ == this) {
    // This is the inner-most RunLoop, so quit now.
    loop_->QuitNow();
  }
}

base::Closure RunLoop::QuitClosure() {
  return base::Bind(&RunLoop::Quit, weak_factory_.GetWeakPtr());
}

bool RunLoop::BeforeRun() {
  DCHECK(!run_called_);
  run_called_ = true;

  // Allow Quit to be called before Run.
  if (quit_called_)
    return false;

  // Push RunLoop stack:
  previous_run_loop_ = loop_->run_loop_;
  run_depth_ = previous_run_loop_? previous_run_loop_->run_depth_ + 1 : 1;
  loop_->run_loop_ = this;

  running_ = true;
  return true;
}

void RunLoop::AfterRun() {
  running_ = false;

  // Pop RunLoop stack:
  loop_->run_loop_ = previous_run_loop_;

  // Execute deferred QuitNow, if any:
  if (previous_run_loop_ && previous_run_loop_->quit_called_)
    loop_->QuitNow();
}

}  // namespace base
