// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

#include <android/log.h>
#include <unwind.h>

#include "base/debug/proc_maps_linux.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"

namespace {

struct StackCrawlState {
  StackCrawlState(uintptr_t* frames, size_t max_depth)
      : frames(frames),
        frame_count(0),
        max_depth(max_depth),
        have_skipped_self(false) {}

  uintptr_t* frames;
  size_t frame_count;
  size_t max_depth;
  bool have_skipped_self;
};

// Clang's unwind.h doesn't provide _Unwind_GetIP on ARM, refer to
// http://llvm.org/bugs/show_bug.cgi?id=16564 for details.
#if defined(__clang__)
uintptr_t _Unwind_GetIP(_Unwind_Context* context) {
  uintptr_t ip = 0;
  _Unwind_VRS_Get(context, _UVRSC_CORE, 15, _UVRSD_UINT32, &ip);
  return ip & ~static_cast<uintptr_t>(0x1);  // Remove thumb mode bit.
}
#endif

_Unwind_Reason_Code TraceStackFrame(_Unwind_Context* context, void* arg) {
  StackCrawlState* state = static_cast<StackCrawlState*>(arg);
  uintptr_t ip = _Unwind_GetIP(context);

  // The first stack frame is this function itself.  Skip it.
  if (ip != 0 && !state->have_skipped_self) {
    state->have_skipped_self = true;
    return _URC_NO_REASON;
  }

  state->frames[state->frame_count++] = ip;
  if (state->frame_count >= state->max_depth)
    return _URC_END_OF_STACK;
  return _URC_NO_REASON;
}

}  // namespace

namespace base {
namespace debug {

bool EnableInProcessStackDumping() {
  // When running in an application, our code typically expects SIGPIPE
  // to be ignored.  Therefore, when testing that same code, it should run
  // with SIGPIPE ignored as well.
  // TODO(phajdan.jr): De-duplicate this SIGPIPE code.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigemptyset(&action.sa_mask);
  return (sigaction(SIGPIPE, &action, NULL) == 0);
}

StackTrace::StackTrace() {
  StackCrawlState state(reinterpret_cast<uintptr_t*>(trace_), kMaxTraces);
  _Unwind_Backtrace(&TraceStackFrame, &state);
  count_ = state.frame_count;
}

void StackTrace::PrintBacktrace() const {
  std::string backtrace = ToString();
  __android_log_write(ANDROID_LOG_ERROR, "chromium", backtrace.c_str());
}

// NOTE: Native libraries in APKs are stripped before installing. Print out the
// relocatable address and library names so host computers can use tools to
// symbolize and demangle (e.g., addr2line, c++filt).
void StackTrace::OutputToStream(std::ostream* os) const {
  std::string proc_maps;
  std::vector<MappedMemoryRegion> regions;
  // Allow IO to read /proc/self/maps. Reading this file doesn't hit the disk
  // since it lives in procfs, and this is currently used to print a stack trace
  // on fatal log messages in debug builds only. If the restriction is enabled
  // then it will recursively trigger fatal failures when this enters on the
  // UI thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (!ReadProcMaps(&proc_maps)) {
    __android_log_write(
        ANDROID_LOG_ERROR, "chromium", "Failed to read /proc/self/maps");
  } else if (!ParseProcMaps(proc_maps, &regions)) {
    __android_log_write(
        ANDROID_LOG_ERROR, "chromium", "Failed to parse /proc/self/maps");
  }

  for (size_t i = 0; i < count_; ++i) {
    // Subtract one as return address of function may be in the next
    // function when a function is annotated as noreturn.
    uintptr_t address = reinterpret_cast<uintptr_t>(trace_[i]) - 1;

    std::vector<MappedMemoryRegion>::iterator iter = regions.begin();
    while (iter != regions.end()) {
      if (address >= iter->start && address < iter->end &&
          !iter->path.empty()) {
        break;
      }
      ++iter;
    }

    *os << base::StringPrintf("#%02d 0x%08x ", i, address);

    if (iter != regions.end()) {
      uintptr_t rel_pc = address - iter->start + iter->offset;
      const char* path = iter->path.c_str();
      *os << base::StringPrintf("%s+0x%08x", path, rel_pc);
    } else {
      *os << "<unknown>";
    }

    *os << "\n";
  }
}

}  // namespace debug
}  // namespace base
