// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Test Lock() and Unlock() functionalities.
TEST(DiscardableMemoryTest, LockAndUnLock) {
  ASSERT_TRUE(DiscardableMemory::Supported());

  const size_t size = 1024;

  DiscardableMemory memory;
  ASSERT_TRUE(memory.InitializeAndLock(size));
  void* addr = memory.Memory();
  ASSERT_NE(static_cast<void*>(NULL), addr);

  memory.Unlock();
  // The system should have no reason to purge discardable blocks in this brief
  // interval, though technically speaking this might flake.
  EXPECT_EQ(DISCARDABLE_MEMORY_SUCCESS, memory.Lock());
  addr = memory.Memory();
  ASSERT_NE(static_cast<void*>(NULL), addr);

  memory.Unlock();
}

// Test delete a discardable memory while it is locked.
TEST(DiscardableMemoryTest, DeleteWhileLocked) {
  ASSERT_TRUE(DiscardableMemory::Supported());

  const size_t size = 1024;

  DiscardableMemory memory;
  ASSERT_TRUE(memory.InitializeAndLock(size));
}

#if defined(OS_MACOSX)
// Test forced purging.
TEST(DiscardableMemoryTest, Purge) {
  ASSERT_TRUE(DiscardableMemory::Supported());
  ASSERT_TRUE(DiscardableMemory::PurgeForTestingSupported());

  const size_t size = 1024;

  DiscardableMemory memory;
  ASSERT_TRUE(memory.InitializeAndLock(size));
  memory.Unlock();

  DiscardableMemory::PurgeForTesting();
  EXPECT_EQ(DISCARDABLE_MEMORY_PURGED, memory.Lock());
}
#endif  // OS_MACOSX

#endif  // OS_*

}
