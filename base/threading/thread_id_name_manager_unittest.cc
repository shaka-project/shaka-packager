// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_id_name_manager.h"

#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest ThreadIdNameManagerTest;

namespace {

static const char* kAThread = "a thread";
static const char* kBThread = "b thread";

TEST_F(ThreadIdNameManagerTest, AddThreads) {
  base::ThreadIdNameManager* manager = base::ThreadIdNameManager::GetInstance();
  base::Thread thread_a(kAThread);
  base::Thread thread_b(kBThread);

  thread_a.Start();
  thread_b.Start();

  EXPECT_STREQ(kAThread, manager->GetName(thread_a.thread_id()));
  EXPECT_STREQ(kBThread, manager->GetName(thread_b.thread_id()));

  thread_b.Stop();
  thread_a.Stop();
}

TEST_F(ThreadIdNameManagerTest, RemoveThreads) {
  base::ThreadIdNameManager* manager = base::ThreadIdNameManager::GetInstance();
  base::Thread thread_a(kAThread);

  thread_a.Start();
  {
    base::Thread thread_b(kBThread);
    thread_b.Start();
    thread_b.Stop();
  }
  EXPECT_STREQ(kAThread, manager->GetName(thread_a.thread_id()));

  thread_a.Stop();
  EXPECT_STREQ("", manager->GetName(thread_a.thread_id()));
}

TEST_F(ThreadIdNameManagerTest, RestartThread) {
  base::ThreadIdNameManager* manager = base::ThreadIdNameManager::GetInstance();
  base::Thread thread_a(kAThread);

  thread_a.Start();
  base::PlatformThreadId a_id = thread_a.thread_id();
  EXPECT_STREQ(kAThread, manager->GetName(a_id));
  thread_a.Stop();

  thread_a.Start();
  EXPECT_STREQ("", manager->GetName(a_id));
  EXPECT_STREQ(kAThread, manager->GetName(thread_a.thread_id()));
  thread_a.Stop();
}

TEST_F(ThreadIdNameManagerTest, ThreadNameInterning) {
  base::ThreadIdNameManager* manager = base::ThreadIdNameManager::GetInstance();

  base::PlatformThreadId a_id = base::PlatformThread::CurrentId();
  base::PlatformThread::SetName("First Name");
  std::string version = manager->GetName(a_id);

  base::PlatformThread::SetName("New name");
  EXPECT_NE(version, manager->GetName(a_id));
  base::PlatformThread::SetName("");
}

TEST_F(ThreadIdNameManagerTest, ResettingNameKeepsCorrectInternedValue) {
  base::ThreadIdNameManager* manager = base::ThreadIdNameManager::GetInstance();

  base::PlatformThreadId a_id = base::PlatformThread::CurrentId();
  base::PlatformThread::SetName("Test Name");
  std::string version = manager->GetName(a_id);

  base::PlatformThread::SetName("New name");
  EXPECT_NE(version, manager->GetName(a_id));

  base::PlatformThread::SetName("Test Name");
  EXPECT_EQ(version, manager->GetName(a_id));

  base::PlatformThread::SetName("");
}

}  // namespace
