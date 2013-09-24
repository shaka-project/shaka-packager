// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/synchronization/waitable_event.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace base {

namespace {

// We use this thread-local variable to record whether or not a thread exited
// because its Stop method was called.  This allows us to catch cases where
// MessageLoop::QuitWhenIdle() is called directly, which is unexpected when
// using a Thread to setup and run a MessageLoop.
base::LazyInstance<base::ThreadLocalBoolean> lazy_tls_bool =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// This is used to trigger the message loop to exit.
void ThreadQuitHelper() {
  MessageLoop::current()->QuitWhenIdle();
  Thread::SetThreadWasQuitProperly(true);
}

// Used to pass data to ThreadMain.  This structure is allocated on the stack
// from within StartWithOptions.
struct Thread::StartupData {
  // We get away with a const reference here because of how we are allocated.
  const Thread::Options& options;

  // Used to synchronize thread startup.
  WaitableEvent event;

  explicit StartupData(const Options& opt)
      : options(opt),
        event(false, false) {}
};

Thread::Thread(const char* name)
    :
#if defined(OS_WIN)
      com_status_(NONE),
#endif
      started_(false),
      stopping_(false),
      running_(false),
      startup_data_(NULL),
      thread_(0),
      message_loop_(NULL),
      thread_id_(kInvalidThreadId),
      name_(name) {
}

Thread::~Thread() {
  Stop();
}

bool Thread::Start() {
  Options options;
#if defined(OS_WIN)
  if (com_status_ == STA)
    options.message_loop_type = MessageLoop::TYPE_UI;
#endif
  return StartWithOptions(options);
}

bool Thread::StartWithOptions(const Options& options) {
  DCHECK(!message_loop_);
#if defined(OS_WIN)
  DCHECK((com_status_ != STA) ||
      (options.message_loop_type == MessageLoop::TYPE_UI));
#endif

  SetThreadWasQuitProperly(false);

  StartupData startup_data(options);
  startup_data_ = &startup_data;

  if (!PlatformThread::Create(options.stack_size, this, &thread_)) {
    DLOG(ERROR) << "failed to create thread";
    startup_data_ = NULL;
    return false;
  }

  // Wait for the thread to start and initialize message_loop_
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  startup_data.event.Wait();

  // set it to NULL so we don't keep a pointer to some object on the stack.
  startup_data_ = NULL;
  started_ = true;

  DCHECK(message_loop_);
  return true;
}

void Thread::Stop() {
  if (!started_)
    return;

  StopSoon();

  // Wait for the thread to exit.
  //
  // TODO(darin): Unfortunately, we need to keep message_loop_ around until
  // the thread exits.  Some consumers are abusing the API.  Make them stop.
  //
  PlatformThread::Join(thread_);

  // The thread should NULL message_loop_ on exit.
  DCHECK(!message_loop_);

  // The thread no longer needs to be joined.
  started_ = false;

  stopping_ = false;
}

void Thread::StopSoon() {
  // We should only be called on the same thread that started us.

  // Reading thread_id_ without a lock can lead to a benign data race
  // with ThreadMain, so we annotate it to stay silent under ThreadSanitizer.
  DCHECK_NE(ANNOTATE_UNPROTECTED_READ(thread_id_), PlatformThread::CurrentId());

  if (stopping_ || !message_loop_)
    return;

  stopping_ = true;
  message_loop_->PostTask(FROM_HERE, base::Bind(&ThreadQuitHelper));
}

bool Thread::IsRunning() const {
  return running_;
}

void Thread::SetPriority(ThreadPriority priority) {
  // The thread must be started (and id known) for this to be
  // compatible with all platforms.
  DCHECK_NE(thread_id_, kInvalidThreadId);
  PlatformThread::SetThreadPriority(thread_, priority);
}

void Thread::Run(MessageLoop* message_loop) {
  message_loop->Run();
}

void Thread::SetThreadWasQuitProperly(bool flag) {
  lazy_tls_bool.Pointer()->Set(flag);
}

bool Thread::GetThreadWasQuitProperly() {
  bool quit_properly = true;
#ifndef NDEBUG
  quit_properly = lazy_tls_bool.Pointer()->Get();
#endif
  return quit_properly;
}

void Thread::ThreadMain() {
  {
    // The message loop for this thread.
    MessageLoop message_loop(startup_data_->options.message_loop_type);

    // Complete the initialization of our Thread object.
    thread_id_ = PlatformThread::CurrentId();
    PlatformThread::SetName(name_.c_str());
    ANNOTATE_THREAD_NAME(name_.c_str());  // Tell the name to race detector.
    message_loop.set_thread_name(name_);
    message_loop_ = &message_loop;

#if defined(OS_WIN)
    scoped_ptr<win::ScopedCOMInitializer> com_initializer;
    if (com_status_ != NONE) {
      com_initializer.reset((com_status_ == STA) ?
          new win::ScopedCOMInitializer() :
          new win::ScopedCOMInitializer(win::ScopedCOMInitializer::kMTA));
    }
#endif

    // Let the thread do extra initialization.
    // Let's do this before signaling we are started.
    Init();

    running_ = true;
    startup_data_->event.Signal();
    // startup_data_ can't be touched anymore since the starting thread is now
    // unlocked.

    Run(message_loop_);
    running_ = false;

    // Let the thread do extra cleanup.
    CleanUp();

#if defined(OS_WIN)
    com_initializer.reset();
#endif

    // Assert that MessageLoop::Quit was called by ThreadQuitHelper.
    DCHECK(GetThreadWasQuitProperly());

    // We can't receive messages anymore.
    message_loop_ = NULL;
  }
}

}  // namespace base
