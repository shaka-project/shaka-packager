// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains intentional memory errors, some of which may lead to
// crashes if the test is ran without special memory testing tools. We use these
// errors to verify the sanity of the tools.

#include "base/atomicops.h"
#include "base/message_loop/message_loop.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const base::subtle::Atomic32 kMagicValue = 42;

// Helper for memory accesses that can potentially corrupt memory or cause a
// crash during a native run.
#if defined(ADDRESS_SANITIZER)
#if defined(OS_IOS)
// EXPECT_DEATH is not supported on IOS.
#define HARMFUL_ACCESS(action,error_regexp) do { action; } while (0)
#else
#define HARMFUL_ACCESS(action,error_regexp) EXPECT_DEATH(action,error_regexp)
#endif  // !OS_IOS
#else
#define HARMFUL_ACCESS(action,error_regexp) \
do { if (RunningOnValgrind()) { action; } } while (0)
#endif

void ReadUninitializedValue(char *ptr) {
  // Comparison with 64 is to prevent clang from optimizing away the
  // jump -- valgrind only catches jumps and conditional moves, but clang uses
  // the borrow flag if the condition is just `*ptr == '\0'`.
  if (*ptr == 64) {
    (*ptr)++;
  } else {
    (*ptr)--;
  }
}

void ReadValueOutOfArrayBoundsLeft(char *ptr) {
  char c = ptr[-2];
  VLOG(1) << "Reading a byte out of bounds: " << c;
}

void ReadValueOutOfArrayBoundsRight(char *ptr, size_t size) {
  char c = ptr[size + 1];
  VLOG(1) << "Reading a byte out of bounds: " << c;
}

// This is harmless if you run it under Valgrind thanks to redzones.
void WriteValueOutOfArrayBoundsLeft(char *ptr) {
  ptr[-1] = kMagicValue;
}

// This is harmless if you run it under Valgrind thanks to redzones.
void WriteValueOutOfArrayBoundsRight(char *ptr, size_t size) {
  ptr[size] = kMagicValue;
}

void MakeSomeErrors(char *ptr, size_t size) {
  ReadUninitializedValue(ptr);
  HARMFUL_ACCESS(ReadValueOutOfArrayBoundsLeft(ptr),
                 "heap-buffer-overflow.*2 bytes to the left");
  HARMFUL_ACCESS(ReadValueOutOfArrayBoundsRight(ptr, size),
                 "heap-buffer-overflow.*1 bytes to the right");
  HARMFUL_ACCESS(WriteValueOutOfArrayBoundsLeft(ptr),
                 "heap-buffer-overflow.*1 bytes to the left");
  HARMFUL_ACCESS(WriteValueOutOfArrayBoundsRight(ptr, size),
                 "heap-buffer-overflow.*0 bytes to the right");
}

}  // namespace

// A memory leak detector should report an error in this test.
TEST(ToolsSanityTest, MemoryLeak) {
  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile leak = new int[256];  // Leak some memory intentionally.
  leak[4] = 1;  // Make sure the allocated memory is used.
}

#if defined(ADDRESS_SANITIZER) && (defined(OS_IOS) || defined(OS_WIN))
// Because iOS doesn't support death tests, each of the following tests will
// crash the whole program under Asan. On Windows Asan is based on SyzyAsan, the
// error report mecanism is different than with Asan so those test will fail.
#define MAYBE_AccessesToNewMemory DISABLED_AccessesToNewMemory
#define MAYBE_AccessesToMallocMemory DISABLED_AccessesToMallocMemory
#else
#define MAYBE_AccessesToNewMemory AccessesToNewMemory
#define MAYBE_AccessesToMallocMemory AccessesToMallocMemory
#define MAYBE_ArrayDeletedWithoutBraces ArrayDeletedWithoutBraces
#define MAYBE_SingleElementDeletedWithBraces SingleElementDeletedWithBraces
#endif

// The following tests pass with Clang r170392, but not r172454, which
// makes AddressSanitizer detect errors in them. We disable these tests under
// AddressSanitizer until we fully switch to Clang r172454. After that the
// tests should be put back under the (defined(OS_IOS) || defined(OS_WIN))
// clause above.
// See also http://crbug.com/172614.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_SingleElementDeletedWithBraces \
    DISABLED_SingleElementDeletedWithBraces
#define MAYBE_ArrayDeletedWithoutBraces DISABLED_ArrayDeletedWithoutBraces
#endif
TEST(ToolsSanityTest, MAYBE_AccessesToNewMemory) {
  char *foo = new char[10];
  MakeSomeErrors(foo, 10);
  delete [] foo;
  // Use after delete.
  HARMFUL_ACCESS(foo[5] = 0, "heap-use-after-free");
}

TEST(ToolsSanityTest, MAYBE_AccessesToMallocMemory) {
  char *foo = reinterpret_cast<char*>(malloc(10));
  MakeSomeErrors(foo, 10);
  free(foo);
  // Use after free.
  HARMFUL_ACCESS(foo[5] = 0, "heap-use-after-free");
}

TEST(ToolsSanityTest, MAYBE_ArrayDeletedWithoutBraces) {
#if !defined(ADDRESS_SANITIZER)
  // This test may corrupt memory if not run under Valgrind or compiled with
  // AddressSanitizer.
  if (!RunningOnValgrind())
    return;
#endif

  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile foo = new int[10];
  delete foo;
}

TEST(ToolsSanityTest, MAYBE_SingleElementDeletedWithBraces) {
#if !defined(ADDRESS_SANITIZER)
  // This test may corrupt memory if not run under Valgrind or compiled with
  // AddressSanitizer.
  if (!RunningOnValgrind())
    return;
#endif

  // Without the |volatile|, clang optimizes away the next two lines.
  int* volatile foo = new int;
  (void) foo;
  delete [] foo;
}

#if defined(ADDRESS_SANITIZER)
TEST(ToolsSanityTest, DISABLED_AddressSanitizerNullDerefCrashTest) {
  // Intentionally crash to make sure AddressSanitizer is running.
  // This test should not be ran on bots.
  int* volatile zero = NULL;
  *zero = 0;
}

TEST(ToolsSanityTest, DISABLED_AddressSanitizerLocalOOBCrashTest) {
  // Intentionally crash to make sure AddressSanitizer is instrumenting
  // the local variables.
  // This test should not be ran on bots.
  int array[5];
  // Work around the OOB warning reported by Clang.
  int* volatile access = &array[5];
  *access = 43;
}

namespace {
int g_asan_test_global_array[10];
}  // namespace

TEST(ToolsSanityTest, DISABLED_AddressSanitizerGlobalOOBCrashTest) {
  // Intentionally crash to make sure AddressSanitizer is instrumenting
  // the global variables.
  // This test should not be ran on bots.

  // Work around the OOB warning reported by Clang.
  int* volatile access = g_asan_test_global_array - 1;
  *access = 43;
}

#endif

namespace {

// We use caps here just to ensure that the method name doesn't interfere with
// the wildcarded suppressions.
class TOOLS_SANITY_TEST_CONCURRENT_THREAD : public PlatformThread::Delegate {
 public:
  explicit TOOLS_SANITY_TEST_CONCURRENT_THREAD(bool *value) : value_(value) {}
  virtual ~TOOLS_SANITY_TEST_CONCURRENT_THREAD() {}
  virtual void ThreadMain() OVERRIDE {
    *value_ = true;

    // Sleep for a few milliseconds so the two threads are more likely to live
    // simultaneously. Otherwise we may miss the report due to mutex
    // lock/unlock's inside thread creation code in pure-happens-before mode...
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  }
 private:
  bool *value_;
};

class ReleaseStoreThread : public PlatformThread::Delegate {
 public:
  explicit ReleaseStoreThread(base::subtle::Atomic32 *value) : value_(value) {}
  virtual ~ReleaseStoreThread() {}
  virtual void ThreadMain() OVERRIDE {
    base::subtle::Release_Store(value_, kMagicValue);

    // Sleep for a few milliseconds so the two threads are more likely to live
    // simultaneously. Otherwise we may miss the report due to mutex
    // lock/unlock's inside thread creation code in pure-happens-before mode...
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
  }
 private:
  base::subtle::Atomic32 *value_;
};

class AcquireLoadThread : public PlatformThread::Delegate {
 public:
  explicit AcquireLoadThread(base::subtle::Atomic32 *value) : value_(value) {}
  virtual ~AcquireLoadThread() {}
  virtual void ThreadMain() OVERRIDE {
    // Wait for the other thread to make Release_Store
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(100));
    base::subtle::Acquire_Load(value_);
  }
 private:
  base::subtle::Atomic32 *value_;
};

void RunInParallel(PlatformThread::Delegate *d1, PlatformThread::Delegate *d2) {
  PlatformThreadHandle a;
  PlatformThreadHandle b;
  PlatformThread::Create(0, d1, &a);
  PlatformThread::Create(0, d2, &b);
  PlatformThread::Join(a);
  PlatformThread::Join(b);
}

}  // namespace

// A data race detector should report an error in this test.
TEST(ToolsSanityTest, DataRace) {
  bool *shared = new bool(false);
  TOOLS_SANITY_TEST_CONCURRENT_THREAD thread1(shared), thread2(shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_TRUE(*shared);
  delete shared;
}

TEST(ToolsSanityTest, AnnotateBenignRace) {
  bool shared = false;
  ANNOTATE_BENIGN_RACE(&shared, "Intentional race - make sure doesn't show up");
  TOOLS_SANITY_TEST_CONCURRENT_THREAD thread1(&shared), thread2(&shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_TRUE(shared);
}

TEST(ToolsSanityTest, AtomicsAreIgnored) {
  base::subtle::Atomic32 shared = 0;
  ReleaseStoreThread thread1(&shared);
  AcquireLoadThread thread2(&shared);
  RunInParallel(&thread1, &thread2);
  EXPECT_EQ(kMagicValue, shared);
}

}  // namespace base
