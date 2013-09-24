// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/activity_status.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace android {

namespace {

using base::android::ScopedJavaLocalRef;

// An invalid ActivityState value.
const ActivityState kInvalidActivityState = static_cast<ActivityState>(100);

// Used to generate a callback that stores the new state at a given location.
void StoreStateTo(ActivityState* target, ActivityState state) {
  *target = state;
}

void RunTasksUntilIdle() {
  RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// Shared state for the multi-threaded test.
// This uses a thread to register for events and listen to them, while state
// changes are forced on the main thread.
class MultiThreadedTest {
 public:
  MultiThreadedTest()
      : activity_status_(ActivityStatus::GetInstance()),
        state_(kInvalidActivityState),
        event_(false, false),
        thread_("ActivityStatusTest thread"),
        main_() {
  }

  void Run() {
    // Start the thread and tell it to register for events.
    thread_.Start();
    thread_.message_loop()
        ->PostTask(FROM_HERE,
                   base::Bind(&MultiThreadedTest::RegisterThreadForEvents,
                              base::Unretained(this)));

    // Wait for its completion.
    event_.Wait();

    // Change state, then wait for the thread to modify state.
    activity_status_->OnActivityStateChange(ACTIVITY_STATE_CREATED);
    event_.Wait();
    EXPECT_EQ(ACTIVITY_STATE_CREATED, state_);

    // Again
    activity_status_->OnActivityStateChange(ACTIVITY_STATE_DESTROYED);
    event_.Wait();
    EXPECT_EQ(ACTIVITY_STATE_DESTROYED, state_);
  }

 private:
  void ExpectOnThread() {
    EXPECT_EQ(thread_.message_loop(), base::MessageLoop::current());
  }

  void RegisterThreadForEvents() {
    ExpectOnThread();
    listener_.reset(new ActivityStatus::Listener(base::Bind(
        &MultiThreadedTest::StoreStateAndSignal, base::Unretained(this))));
    EXPECT_TRUE(listener_.get());
    event_.Signal();
  }

  void StoreStateAndSignal(ActivityState state) {
    ExpectOnThread();
    state_ = state;
    event_.Signal();
  }

  ActivityStatus* const activity_status_;
  ActivityState state_;
  base::WaitableEvent event_;
  base::Thread thread_;
  base::MessageLoop main_;
  scoped_ptr<ActivityStatus::Listener> listener_;
};

}  // namespace

TEST(ActivityStatusTest, SingleThread) {
  MessageLoop message_loop;

  ActivityState result = kInvalidActivityState;

  // Create a new listener that stores the new state into |result| on every
  // state change.
  ActivityStatus::Listener listener(
      base::Bind(&StoreStateTo, base::Unretained(&result)));

  EXPECT_EQ(kInvalidActivityState, result);

  ActivityStatus* const activity_status = ActivityStatus::GetInstance();
  activity_status->OnActivityStateChange(ACTIVITY_STATE_CREATED);
  RunTasksUntilIdle();
  EXPECT_EQ(ACTIVITY_STATE_CREATED, result);

  activity_status->OnActivityStateChange(ACTIVITY_STATE_DESTROYED);
  RunTasksUntilIdle();
  EXPECT_EQ(ACTIVITY_STATE_DESTROYED, result);
}

TEST(ActivityStatusTest, TwoThreads) {
  MultiThreadedTest test;
  test.Run();
}

}  // namespace android
}  // namespace base
