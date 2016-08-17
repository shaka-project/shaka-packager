// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "packager/base/bind.h"
#include "packager/base/synchronization/waitable_event.h"
#include "packager/media/base/closure_thread.h"

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;

namespace {

const char kThreadNamePrefix[] = "TestClosureThread";

// Mock operation for testing.
class MockOperation {
 public:
  MOCK_METHOD0(DoSomething, bool());
};

}  // namespace

namespace shaka {
namespace media {

class ClosureThreadTest : public ::testing::Test {
 public:
  ClosureThreadTest()
      : thread_(
            new ClosureThread(kThreadNamePrefix,
                              base::Bind(&ClosureThreadTest::ClosureCallback,
                                         base::Unretained(this)))),
        val_(0) {}

  ~ClosureThreadTest() override {}

  void ClosureCallback() {
    // Exit the loop if DoSomething return false.
    while (operation_.DoSomething())
      continue;
  }

  void IncrementVal() { ++val_; }

 protected:
  int val() { return val_; }
  void set_val(int val) { val_ = val; }

  MockOperation operation_;
  std::unique_ptr<ClosureThread> thread_;

 private:
  int val_;

  DISALLOW_COPY_AND_ASSIGN(ClosureThreadTest);
};

TEST_F(ClosureThreadTest, Basic) {
  EXPECT_CALL(operation_, DoSomething()).WillOnce(Return(false));

  ASSERT_EQ(kThreadNamePrefix, thread_->name_prefix());
  ASSERT_FALSE(thread_->HasBeenStarted());
  thread_->Start();
  ASSERT_TRUE(thread_->HasBeenStarted());
  thread_->Join();
}

TEST_F(ClosureThreadTest, CheckInteraction) {
  base::WaitableEvent event_in_thread(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent event_in_main(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  set_val(8);

  // Expect the operation to be invoked twice:
  //   1) When invoked for the first time, increment the value then wait
  //      for the signal from main event and return true to continue;
  //   2) When invoked for the second time, increment the value again and
  //      return true to quit the closure loop.
  EXPECT_CALL(operation_, DoSomething())
      .WillOnce(DoAll(Invoke(this, &ClosureThreadTest::IncrementVal),
                      Invoke(&event_in_main, &base::WaitableEvent::Signal),
                      Invoke(&event_in_thread, &base::WaitableEvent::Wait),
                      Return(true)))
      .WillOnce(DoAll(Invoke(this, &ClosureThreadTest::IncrementVal),
                      Return(false)));

  thread_->Start();

  // Wait until |thread_| signals the main thread.
  event_in_main.Wait();
  EXPECT_EQ(9, val());

  // Signal |thread_| to continue.
  event_in_thread.Signal();
  thread_->Join();
  EXPECT_EQ(10, val());
}

TEST_F(ClosureThreadTest, NotJoined) {
  EXPECT_CALL(operation_, DoSomething()).WillOnce(Return(false));

  thread_->Start();
  // Destroy the thread. The thread should be joined automatically.
  thread_.reset();
}

// Expect death if the thread is destroyed without being started.
// Note: This test is disabled since it causes memory leak in Debug mode as the
// thread object cannot be cleaned up due to the expected death.
TEST_F(ClosureThreadTest, DISABLED_NotStarted) {
  ASSERT_FALSE(thread_->HasBeenStarted());
  ClosureThread* thread = thread_.release();
  EXPECT_DEBUG_DEATH(delete thread, ".*Check failed: HasBeenStarted.*");
}

}  // namespace media
}  // namespace shaka
