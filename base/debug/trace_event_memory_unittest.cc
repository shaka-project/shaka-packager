// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_memory.h"

#include <sstream>
#include <string>

#include "base/debug/trace_event_impl.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(TCMALLOC_TRACE_MEMORY_SUPPORTED)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif

namespace base {
namespace debug {

// Tests for the trace event memory tracking system. Exists as a class so it
// can be a friend of TraceMemoryController.
class TraceMemoryTest : public testing::Test {
 public:
  TraceMemoryTest() {}
  virtual ~TraceMemoryTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TraceMemoryTest);
};

//////////////////////////////////////////////////////////////////////////////

#if defined(TCMALLOC_TRACE_MEMORY_SUPPORTED)

TEST_F(TraceMemoryTest, TraceMemoryController) {
  MessageLoop message_loop;

  // Start with no observers of the TraceLog.
  EXPECT_EQ(0u, TraceLog::GetInstance()->GetObserverCountForTest());

  // Creating a controller adds it to the TraceLog observer list.
  scoped_ptr<TraceMemoryController> controller(
      new TraceMemoryController(
          message_loop.message_loop_proxy(),
          ::HeapProfilerWithPseudoStackStart,
          ::HeapProfilerStop,
          ::GetHeapProfile));
  EXPECT_EQ(1u, TraceLog::GetInstance()->GetObserverCountForTest());
  EXPECT_TRUE(
      TraceLog::GetInstance()->HasEnabledStateObserver(controller.get()));

  // By default the observer isn't dumping memory profiles.
  EXPECT_FALSE(controller->IsTimerRunningForTest());

  // Simulate enabling tracing.
  controller->StartProfiling();
  message_loop.RunUntilIdle();
  EXPECT_TRUE(controller->IsTimerRunningForTest());

  // Simulate disabling tracing.
  controller->StopProfiling();
  message_loop.RunUntilIdle();
  EXPECT_FALSE(controller->IsTimerRunningForTest());

  // Deleting the observer removes it from the TraceLog observer list.
  controller.reset();
  EXPECT_EQ(0u, TraceLog::GetInstance()->GetObserverCountForTest());
}

TEST_F(TraceMemoryTest, ScopedTraceMemory) {
  ScopedTraceMemory::InitForTest();

  // Start with an empty stack.
  EXPECT_EQ(0, ScopedTraceMemory::GetStackIndexForTest());

  {
    // Push an item.
    const char kScope1[] = "scope1";
    ScopedTraceMemory scope1(kScope1);
    EXPECT_EQ(1, ScopedTraceMemory::GetStackIndexForTest());
    EXPECT_EQ(kScope1, ScopedTraceMemory::GetItemForTest(0));

    {
      // One more item.
      const char kScope2[] = "scope2";
      ScopedTraceMemory scope2(kScope2);
      EXPECT_EQ(2, ScopedTraceMemory::GetStackIndexForTest());
      EXPECT_EQ(kScope2, ScopedTraceMemory::GetItemForTest(1));
    }

    // Ended scope 2.
    EXPECT_EQ(1, ScopedTraceMemory::GetStackIndexForTest());
  }

  // Ended scope 1.
  EXPECT_EQ(0, ScopedTraceMemory::GetStackIndexForTest());

  ScopedTraceMemory::CleanupForTest();
}

void TestDeepScopeNesting(int current, int depth) {
  EXPECT_EQ(current, ScopedTraceMemory::GetStackIndexForTest());
  const char kCategory[] = "foo";
  ScopedTraceMemory scope(kCategory);
  if (current < depth)
    TestDeepScopeNesting(current + 1, depth);
  EXPECT_EQ(current + 1, ScopedTraceMemory::GetStackIndexForTest());
}

TEST_F(TraceMemoryTest, DeepScopeNesting) {
  ScopedTraceMemory::InitForTest();

  // Ensure really deep scopes don't crash.
  TestDeepScopeNesting(0, 100);

  ScopedTraceMemory::CleanupForTest();
}

#endif  // defined(TRACE_MEMORY_SUPPORTED)

/////////////////////////////////////////////////////////////////////////////

TEST_F(TraceMemoryTest, AppendHeapProfileTotalsAsTraceFormat) {
  // Empty input gives empty output.
  std::string empty_output;
  AppendHeapProfileTotalsAsTraceFormat("", &empty_output);
  EXPECT_EQ("", empty_output);

  // Typical case.
  const char input[] =
      "heap profile:    357:    55227 [ 14653:  2624014] @ heapprofile";
  const std::string kExpectedOutput =
      "{\"current_allocs\": 357, \"current_bytes\": 55227, \"trace\": \"\"}";
  std::string output;
  AppendHeapProfileTotalsAsTraceFormat(input, &output);
  EXPECT_EQ(kExpectedOutput, output);
}

TEST_F(TraceMemoryTest, AppendHeapProfileLineAsTraceFormat) {
  // Empty input gives empty output.
  std::string empty_output;
  EXPECT_FALSE(AppendHeapProfileLineAsTraceFormat("", &empty_output));
  EXPECT_EQ("", empty_output);

  // Invalid input returns false.
  std::string junk_output;
  EXPECT_FALSE(AppendHeapProfileLineAsTraceFormat("junk", &junk_output));

  // Input with the addresses of name1 and name2.
  const char kName1[] = "name1";
  const char kName2[] = "name2";
  std::ostringstream input;
  input << "   68:     4195 [  1087:    98009] @ " << &kName1 << " " << &kName2;
  const std::string kExpectedOutput =
      ",\n"
      "{"
      "\"current_allocs\": 68, "
      "\"current_bytes\": 4195, "
      "\"trace\": \"name1 name2 \""
      "}";
  std::string output;
  EXPECT_TRUE(
      AppendHeapProfileLineAsTraceFormat(input.str().c_str(), &output));
  EXPECT_EQ(kExpectedOutput, output);

  // Zero current allocations is skipped.
  std::ostringstream zero_input;
  zero_input << "   0:     0 [  1087:    98009] @ " << &kName1 << " "
             << &kName2;
  std::string zero_output;
  EXPECT_FALSE(AppendHeapProfileLineAsTraceFormat(zero_input.str().c_str(),
                                                  &zero_output));
  EXPECT_EQ("", zero_output);
}

TEST_F(TraceMemoryTest, AppendHeapProfileAsTraceFormat) {
  // Empty input gives empty output.
  std::string empty_output;
  AppendHeapProfileAsTraceFormat("", &empty_output);
  EXPECT_EQ("", empty_output);

  // Typical case.
  const char input[] =
      "heap profile:    357:    55227 [ 14653:  2624014] @ heapprofile\n"
      "   95:    40940 [   649:   114260] @\n"
      "   77:    32546 [   742:   106234] @ 0x0 0x0\n"
      "    0:        0 [   132:     4236] @ 0x0\n"
      "\n"
      "MAPPED_LIBRARIES:\n"
      "1be411fc1000-1be4139e4000 rw-p 00000000 00:00 0\n"
      "1be4139e4000-1be4139e5000 ---p 00000000 00:00 0\n";
  const std::string kExpectedOutput =
      "[{"
      "\"current_allocs\": 357, "
      "\"current_bytes\": 55227, "
      "\"trace\": \"\"},\n"
      "{\"current_allocs\": 95, "
      "\"current_bytes\": 40940, "
      "\"trace\": \"\"},\n"
      "{\"current_allocs\": 77, "
      "\"current_bytes\": 32546, "
      "\"trace\": \"null null \""
      "}]\n";
  std::string output;
  AppendHeapProfileAsTraceFormat(input, &output);
  EXPECT_EQ(kExpectedOutput, output);
}

}  // namespace debug
}  // namespace base
