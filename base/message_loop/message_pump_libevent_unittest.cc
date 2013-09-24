// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_libevent.h"

#include <unistd.h>

#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libevent/event.h"

namespace base {

class MessagePumpLibeventTest : public testing::Test {
 protected:
  MessagePumpLibeventTest()
      : ui_loop_(MessageLoop::TYPE_UI),
        io_thread_("MessagePumpLibeventTestIOThread") {}
  virtual ~MessagePumpLibeventTest() {}

  virtual void SetUp() OVERRIDE {
    Thread::Options options(MessageLoop::TYPE_IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(options));
    ASSERT_EQ(MessageLoop::TYPE_IO, io_thread_.message_loop()->type());
    int ret = pipe(pipefds_);
    ASSERT_EQ(0, ret);
  }

  virtual void TearDown() OVERRIDE {
    if (HANDLE_EINTR(close(pipefds_[0])) < 0)
      PLOG(ERROR) << "close";
    if (HANDLE_EINTR(close(pipefds_[1])) < 0)
      PLOG(ERROR) << "close";
  }

  MessageLoop* ui_loop() { return &ui_loop_; }
  MessageLoopForIO* io_loop() const {
    return static_cast<MessageLoopForIO*>(io_thread_.message_loop());
  }

  void OnLibeventNotification(
      MessagePumpLibevent* pump,
      MessagePumpLibevent::FileDescriptorWatcher* controller) {
    pump->OnLibeventNotification(0, EV_WRITE | EV_READ, controller);
  }

  int pipefds_[2];

 private:
  MessageLoop ui_loop_;
  Thread io_thread_;
};

namespace {

// Concrete implementation of MessagePumpLibevent::Watcher that does
// nothing useful.
class StupidWatcher : public MessagePumpLibevent::Watcher {
 public:
  virtual ~StupidWatcher() {}

  // base:MessagePumpLibevent::Watcher interface
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {}
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}
};

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)

// Test to make sure that we catch calling WatchFileDescriptor off of the
// wrong thread.
TEST_F(MessagePumpLibeventTest, TestWatchingFromBadThread) {
  MessagePumpLibevent::FileDescriptorWatcher watcher;
  StupidWatcher delegate;

  ASSERT_DEATH(io_loop()->WatchFileDescriptor(
      STDOUT_FILENO, false, MessageLoopForIO::WATCH_READ, &watcher, &delegate),
      "Check failed: "
      "watch_file_descriptor_caller_checker_.CalledOnValidThread()");
}

#endif  // GTEST_HAS_DEATH_TEST && !defined(NDEBUG)

class BaseWatcher : public MessagePumpLibevent::Watcher {
 public:
  explicit BaseWatcher(MessagePumpLibevent::FileDescriptorWatcher* controller)
      : controller_(controller) {
    DCHECK(controller_);
  }
  virtual ~BaseWatcher() {}

  // base:MessagePumpLibevent::Watcher interface
  virtual void OnFileCanReadWithoutBlocking(int /* fd */) OVERRIDE {
    NOTREACHED();
  }

  virtual void OnFileCanWriteWithoutBlocking(int /* fd */) OVERRIDE {
    NOTREACHED();
  }

 protected:
  MessagePumpLibevent::FileDescriptorWatcher* controller_;
};

class DeleteWatcher : public BaseWatcher {
 public:
  explicit DeleteWatcher(
      MessagePumpLibevent::FileDescriptorWatcher* controller)
      : BaseWatcher(controller) {}

  virtual ~DeleteWatcher() {
    DCHECK(!controller_);
  }

  virtual void OnFileCanWriteWithoutBlocking(int /* fd */) OVERRIDE {
    DCHECK(controller_);
    delete controller_;
    controller_ = NULL;
  }
};

TEST_F(MessagePumpLibeventTest, DeleteWatcher) {
  scoped_ptr<MessagePumpLibevent> pump(new MessagePumpLibevent);
  MessagePumpLibevent::FileDescriptorWatcher* watcher =
      new MessagePumpLibevent::FileDescriptorWatcher;
  DeleteWatcher delegate(watcher);
  pump->WatchFileDescriptor(pipefds_[1],
      false, MessagePumpLibevent::WATCH_READ_WRITE, watcher, &delegate);

  // Spoof a libevent notification.
  OnLibeventNotification(pump.get(), watcher);
}

class StopWatcher : public BaseWatcher {
 public:
  explicit StopWatcher(
      MessagePumpLibevent::FileDescriptorWatcher* controller)
      : BaseWatcher(controller) {}

  virtual ~StopWatcher() {}

  virtual void OnFileCanWriteWithoutBlocking(int /* fd */) OVERRIDE {
    controller_->StopWatchingFileDescriptor();
  }
};

TEST_F(MessagePumpLibeventTest, StopWatcher) {
  scoped_ptr<MessagePumpLibevent> pump(new MessagePumpLibevent);
  MessagePumpLibevent::FileDescriptorWatcher watcher;
  StopWatcher delegate(&watcher);
  pump->WatchFileDescriptor(pipefds_[1],
      false, MessagePumpLibevent::WATCH_READ_WRITE, &watcher, &delegate);

  // Spoof a libevent notification.
  OnLibeventNotification(pump.get(), &watcher);
}

}  // namespace

}  // namespace base
