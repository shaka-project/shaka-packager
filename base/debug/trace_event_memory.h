// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_TRACE_EVENT_MEMORY_H_
#define BASE_DEBUG_TRACE_EVENT_MEMORY_H_

#include "base/base_export.h"
#include "base/debug/trace_event_impl.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

// TODO(jamescook): Windows support for memory tracing.
#if !defined(NO_TCMALLOC) && !defined(OS_NACL) && \
    (defined(OS_LINUX) || defined(OS_ANDROID))
#define TCMALLOC_TRACE_MEMORY_SUPPORTED 1
#endif

namespace base {

class MessageLoopProxy;

namespace debug {

// Watches for chrome://tracing to be enabled or disabled. When tracing is
// enabled, also enables tcmalloc heap profiling. This class is the preferred
// way to turn trace-base heap memory profiling on and off.
class BASE_EXPORT TraceMemoryController
    : public TraceLog::EnabledStateObserver {
 public:
  typedef int (*StackGeneratorFunction)(int skip_count, void** stack);
  typedef void (*HeapProfilerStartFunction)(StackGeneratorFunction callback);
  typedef void (*HeapProfilerStopFunction)();
  typedef char* (*GetHeapProfileFunction)();

  // |message_loop_proxy| must be a proxy to the primary thread for the client
  // process, e.g. the UI thread in a browser. The function pointers must be
  // pointers to tcmalloc heap profiling functions; by avoiding direct calls to
  // these functions we avoid a dependency on third_party/tcmalloc from base.
  TraceMemoryController(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      HeapProfilerStartFunction heap_profiler_start_function,
      HeapProfilerStopFunction heap_profiler_stop_function,
      GetHeapProfileFunction get_heap_profile_function);
  virtual ~TraceMemoryController();

  // base::debug::TraceLog::EnabledStateChangedObserver overrides:
  virtual void OnTraceLogEnabled() OVERRIDE;
  virtual void OnTraceLogDisabled() OVERRIDE;

  // Starts heap memory profiling.
  void StartProfiling();

  // Captures a heap profile.
  void DumpMemoryProfile();

  // If memory tracing is enabled, dumps a memory profile to the tracing system.
  void StopProfiling();

 private:
  FRIEND_TEST_ALL_PREFIXES(TraceMemoryTest, TraceMemoryController);

  bool IsTimerRunningForTest() const;

  // Ensures the observer starts and stops tracing on the primary thread.
  scoped_refptr<MessageLoopProxy> message_loop_proxy_;

  // Pointers to tcmalloc heap profiling functions. Allows this class to use
  // tcmalloc functions without introducing a dependency from base to tcmalloc.
  HeapProfilerStartFunction heap_profiler_start_function_;
  HeapProfilerStopFunction heap_profiler_stop_function_;
  GetHeapProfileFunction get_heap_profile_function_;

  // Timer to schedule memory profile dumps.
  RepeatingTimer<TraceMemoryController> dump_timer_;

  WeakPtrFactory<TraceMemoryController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TraceMemoryController);
};

//////////////////////////////////////////////////////////////////////////////

// A scoped context for memory tracing. Pushes the name onto a stack for
// recording by tcmalloc heap profiling.
class BASE_EXPORT ScopedTraceMemory {
 public:
  // Memory for |name| must be static, for example, a literal string in
  // a TRACE_EVENT macro.
  explicit ScopedTraceMemory(const char* name);
  ~ScopedTraceMemory();

  // Enables the storing of trace names on a per-thread stack.
  static void set_enabled(bool enabled) { enabled_ = enabled; }

  // Testing interface:
  static void InitForTest();
  static void CleanupForTest();
  static int GetStackIndexForTest();
  static const char* GetItemForTest(int index);

 private:
  static bool enabled_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTraceMemory);
};

//////////////////////////////////////////////////////////////////////////////

// Converts tcmalloc's heap profiler data with pseudo-stacks in |input| to
// trace event compatible JSON and appends to |output|. Visible for testing.
BASE_EXPORT void AppendHeapProfileAsTraceFormat(const char* input,
                                                std::string* output);

// Converts the first |line| of heap profiler data, which contains totals for
// all allocations in a special format, into trace event compatible JSON and
// appends to |output|. Visible for testing.
BASE_EXPORT void AppendHeapProfileTotalsAsTraceFormat(const std::string& line,
                                                      std::string* output);

// Converts a single |line| of heap profiler data into trace event compatible
// JSON and appends to |output|. Returns true if the line was valid and has a
// non-zero number of current allocations. Visible for testing.
BASE_EXPORT bool AppendHeapProfileLineAsTraceFormat(const std::string& line,
                                                    std::string* output);

}  // namespace debug
}  // namespace base

// Make local variables with unique names based on the line number. Note that
// the extra level of redirection is needed.
#define INTERNAL_TRACE_MEMORY_ID3(line) trace_memory_unique_##line
#define INTERNAL_TRACE_MEMORY_ID2(line) INTERNAL_TRACE_MEMORY_ID3(line)
#define INTERNAL_TRACE_MEMORY_ID INTERNAL_TRACE_MEMORY_ID2(__LINE__)

// This is the core macro that adds a scope to each TRACE_EVENT location.
// It generates a unique local variable name using the macros above.
// TODO(jamescook): Make it record both category and name.
#if defined(TCMALLOC_TRACE_MEMORY_SUPPORTED)
#define INTERNAL_TRACE_MEMORY(category, name) \
  base::debug::ScopedTraceMemory INTERNAL_TRACE_MEMORY_ID(name);
#else
#define INTERNAL_TRACE_MEMORY(category, name)
#endif  // defined(TRACE_MEMORY_SUPPORTED)

// A special trace name that allows us to ignore memory allocations inside
// the memory dump system itself. The allocations are recorded, but the
// visualizer skips them. Must match the value in heap.js.
#define TRACE_MEMORY_IGNORE "trace-memory-ignore"

#endif  // BASE_DEBUG_TRACE_EVENT_MEMORY_H_
