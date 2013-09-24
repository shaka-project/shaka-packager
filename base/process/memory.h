// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_MEMORY_H_
#define BASE_PROCESS_MEMORY_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {

// Enables low fragmentation heap (LFH) for every heaps of this process. This
// won't have any effect on heaps created after this function call. It will not
// modify data allocated in the heaps before calling this function. So it is
// better to call this function early in initialization and again before
// entering the main loop.
// Note: Returns true on Windows 2000 without doing anything.
BASE_EXPORT bool EnableLowFragmentationHeap();

// Enables 'terminate on heap corruption' flag. Helps protect against heap
// overflow. Has no effect if the OS doesn't provide the necessary facility.
BASE_EXPORT void EnableTerminationOnHeapCorruption();

// Turns on process termination if memory runs out.
BASE_EXPORT void EnableTerminationOnOutOfMemory();

#if defined(OS_WIN)
// Returns the module handle to which an address belongs. The reference count
// of the module is not incremented.
BASE_EXPORT HMODULE GetModuleFromAddress(void* address);
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
BASE_EXPORT extern size_t g_oom_size;

// The maximum allowed value for the OOM score.
const int kMaxOomScore = 1000;

// This adjusts /proc/<pid>/oom_score_adj so the Linux OOM killer will
// prefer to kill certain process types over others. The range for the
// adjustment is [-1000, 1000], with [0, 1000] being user accessible.
// If the Linux system doesn't support the newer oom_score_adj range
// of [0, 1000], then we revert to using the older oom_adj, and
// translate the given value into [0, 15].  Some aliasing of values
// may occur in that case, of course.
BASE_EXPORT bool AdjustOOMScore(ProcessId process, int score);
#endif

#if defined(OS_MACOSX)
// Very large images or svg canvases can cause huge mallocs.  Skia
// does tricks on tcmalloc-based systems to allow malloc to fail with
// a NULL rather than hit the oom crasher.  This replicates that for
// OSX.
//
// IF YOU USE THIS WITHOUT CONSULTING YOUR FRIENDLY OSX DEVELOPER,
// YOUR CODE IS LIKELY TO BE REVERTED.  THANK YOU.
BASE_EXPORT void* UncheckedMalloc(size_t size);
#endif  // defined(OS_MACOSX)

}  // namespace base

#endif  // BASE_PROCESS_MEMORY_H_
