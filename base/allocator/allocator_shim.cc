// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim.h"

#include <config.h>
#include "base/allocator/allocator_extension_thunks.h"
#include "base/profiler/alternate_timer.h"
#include "base/sysinfo.h"
#include "jemalloc.h"

// When defined, different heap allocators can be used via an environment
// variable set before running the program.  This may reduce the amount
// of inlining that we get with malloc/free/etc.  Disabling makes it
// so that only tcmalloc can be used.
#define ENABLE_DYNAMIC_ALLOCATOR_SWITCHING

// TODO(mbelshe): Ensure that all calls to tcmalloc have the proper call depth
// from the "user code" so that debugging tools (HeapChecker) can work.

// __THROW is defined in glibc systems.  It means, counter-intuitively,
// "This function will never throw an exception."  It's an optional
// optimization tool, but we may need to use it to match glibc prototypes.
#ifndef __THROW    // I guess we're not on a glibc system
# define __THROW   // __THROW is just an optimization, so ok to make it ""
#endif

// new_mode behaves similarly to MSVC's _set_new_mode.
// If flag is 0 (default), calls to malloc will behave normally.
// If flag is 1, calls to malloc will behave like calls to new,
// and the std_new_handler will be invoked on failure.
// Can be set by calling _set_new_mode().
static int new_mode = 0;

typedef enum {
  TCMALLOC,    // TCMalloc is the default allocator.
  JEMALLOC,    // JEMalloc.
  WINHEAP,  // Windows Heap (standard Windows allocator).
  WINLFH,      // Windows LFH Heap.
} Allocator;

// This is the default allocator. This value can be changed at startup by
// specifying environment variables shown below it.
// See SetupSubprocessAllocator() to specify a default secondary (subprocess)
// allocator.
// TODO(jar): Switch to using TCMALLOC for the renderer as well.
#if (defined(ADDRESS_SANITIZER) && defined(OS_WIN))
// The Windows implementation of Asan requires the use of "WINHEAP".
static Allocator allocator = WINHEAP;
#else
static Allocator allocator = TCMALLOC;
#endif
// The names of the environment variables that can optionally control the
// selection of the allocator.  The primary may be used to control overall
// allocator selection, and the secondary can be used to specify an allocator
// to use in sub-processes.
static const char primary_name[] = "CHROME_ALLOCATOR";
static const char secondary_name[] = "CHROME_ALLOCATOR_2";

// We include tcmalloc and the win_allocator to get as much inlining as
// possible.
#include "debugallocation_shim.cc"
#include "win_allocator.cc"

// Forward declarations from jemalloc.
extern "C" {
void* je_malloc(size_t s);
void* je_realloc(void* p, size_t s);
void je_free(void* s);
size_t je_msize(void* p);
bool je_malloc_init_hard();
void* je_memalign(size_t a, size_t s);
}

// Call the new handler, if one has been set.
// Returns true on successfully calling the handler, false otherwise.
inline bool call_new_handler(bool nothrow) {
  // Get the current new handler.  NB: this function is not
  // thread-safe.  We make a feeble stab at making it so here, but
  // this lock only protects against tcmalloc interfering with
  // itself, not with other libraries calling set_new_handler.
  std::new_handler nh;
  {
    SpinLockHolder h(&set_new_handler_lock);
    nh = std::set_new_handler(0);
    (void) std::set_new_handler(nh);
  }
#if (defined(__GNUC__) && !defined(__EXCEPTIONS)) || \
    (defined(_HAS_EXCEPTIONS) && !_HAS_EXCEPTIONS)
  if (!nh)
    return false;
  // Since exceptions are disabled, we don't really know if new_handler
  // failed.  Assume it will abort if it fails.
  (*nh)();
  return false;  // break out of the retry loop.
#else
  // If no new_handler is established, the allocation failed.
  if (!nh) {
    if (nothrow)
      return false;
    throw std::bad_alloc();
  }
  // Otherwise, try the new_handler.  If it returns, retry the
  // allocation.  If it throws std::bad_alloc, fail the allocation.
  // if it throws something else, don't interfere.
  try {
    (*nh)();
  } catch (const std::bad_alloc&) {
    if (!nothrow)
      throw;
    return true;
  }
#endif  // (defined(__GNUC__) && !defined(__EXCEPTIONS)) || (defined(_HAS_EXCEPTIONS) && !_HAS_EXCEPTIONS)
  return false;
}

extern "C" {
void* malloc(size_t size) __THROW {
  void* ptr;
  for (;;) {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
    switch (allocator) {
      case JEMALLOC:
        ptr = je_malloc(size);
        break;
      case WINHEAP:
      case WINLFH:
        ptr = win_heap_malloc(size);
        break;
      case TCMALLOC:
      default:
        ptr = do_malloc(size);
        break;
    }
#else
    // TCMalloc case.
    ptr = do_malloc(size);
#endif
    if (ptr)
      return ptr;

    if (!new_mode || !call_new_handler(true))
      break;
  }
  return ptr;
}

void free(void* p) __THROW {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  switch (allocator) {
    case JEMALLOC:
      je_free(p);
      return;
    case WINHEAP:
    case WINLFH:
      win_heap_free(p);
      return;
  }
#endif
  // TCMalloc case.
  do_free(p);
}

void* realloc(void* ptr, size_t size) __THROW {
  // Webkit is brittle for allocators that return NULL for malloc(0).  The
  // realloc(0, 0) code path does not guarantee a non-NULL return, so be sure
  // to call malloc for this case.
  if (!ptr)
    return malloc(size);

  void* new_ptr;
  for (;;) {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
    switch (allocator) {
      case JEMALLOC:
        new_ptr = je_realloc(ptr, size);
        break;
      case WINHEAP:
      case WINLFH:
        new_ptr = win_heap_realloc(ptr, size);
        break;
      case TCMALLOC:
      default:
        new_ptr = do_realloc(ptr, size);
        break;
    }
#else
    // TCMalloc case.
    new_ptr = do_realloc(ptr, size);
#endif

    // Subtle warning:  NULL return does not alwas indicate out-of-memory.  If
    // the requested new size is zero, realloc should free the ptr and return
    // NULL.
    if (new_ptr || !size)
      return new_ptr;
    if (!new_mode || !call_new_handler(true))
      break;
  }
  return new_ptr;
}

// TODO(mbelshe): Implement this for other allocators.
void malloc_stats(void) __THROW {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  switch (allocator) {
    case JEMALLOC:
      // No stats.
      return;
    case WINHEAP:
    case WINLFH:
      // No stats.
      return;
  }
#endif
  tc_malloc_stats();
}

#ifdef WIN32

extern "C" size_t _msize(void* p) {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  switch (allocator) {
    case JEMALLOC:
      return je_msize(p);
    case WINHEAP:
    case WINLFH:
      return win_heap_msize(p);
  }
#endif
  return MallocExtension::instance()->GetAllocatedSize(p);
}

// This is included to resolve references from libcmt.
extern "C" intptr_t _get_heap_handle() {
  return 0;
}

static bool get_allocator_waste_size_thunk(size_t* size) {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  switch (allocator) {
    case JEMALLOC:
    case WINHEAP:
    case WINLFH:
      // TODO(alexeif): Implement for allocators other than tcmalloc.
      return false;
  }
#endif
  size_t heap_size, allocated_bytes, unmapped_bytes;
  MallocExtension* ext = MallocExtension::instance();
  if (ext->GetNumericProperty("generic.heap_size", &heap_size) &&
      ext->GetNumericProperty("generic.current_allocated_bytes",
                              &allocated_bytes) &&
      ext->GetNumericProperty("tcmalloc.pageheap_unmapped_bytes",
                              &unmapped_bytes)) {
    *size = heap_size - allocated_bytes - unmapped_bytes;
    return true;
  }
  return false;
}

static void get_stats_thunk(char* buffer, int buffer_length) {
  MallocExtension::instance()->GetStats(buffer, buffer_length);
}

static void release_free_memory_thunk() {
  MallocExtension::instance()->ReleaseFreeMemory();
}

// The CRT heap initialization stub.
extern "C" int _heap_init() {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
// Don't use the environment variable if ADDRESS_SANITIZER is defined on
// Windows, as the implementation requires Winheap to be the allocator.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_WIN))
  const char* environment_value = GetenvBeforeMain(primary_name);
  if (environment_value) {
    if (!stricmp(environment_value, "jemalloc"))
      allocator = JEMALLOC;
    else if (!stricmp(environment_value, "winheap"))
      allocator = WINHEAP;
    else if (!stricmp(environment_value, "winlfh"))
      allocator = WINLFH;
    else if (!stricmp(environment_value, "tcmalloc"))
      allocator = TCMALLOC;
  }
#endif

  switch (allocator) {
    case JEMALLOC:
      return je_malloc_init_hard() ? 0 : 1;
    case WINHEAP:
      return win_heap_init(false) ? 1 : 0;
    case WINLFH:
      return win_heap_init(true) ? 1 : 0;
    case TCMALLOC:
    default:
      // fall through
      break;
  }
#endif
  // Initializing tcmalloc.
  // We intentionally leak this object.  It lasts for the process
  // lifetime.  Trying to teardown at _heap_term() is so late that
  // you can't do anything useful anyway.
  new TCMallocGuard();

  // Provide optional hook for monitoring allocation quantities on a per-thread
  // basis.  Only set the hook if the environment indicates this needs to be
  // enabled.
  const char* profiling =
      GetenvBeforeMain(tracked_objects::kAlternateProfilerTime);
  if (profiling && *profiling == '1') {
    tracked_objects::SetAlternateTimeSource(
        tcmalloc::ThreadCache::GetBytesAllocatedOnCurrentThread,
        tracked_objects::TIME_SOURCE_TYPE_TCMALLOC);
  }

  base::allocator::thunks::SetGetAllocatorWasteSizeFunction(
      get_allocator_waste_size_thunk);
  base::allocator::thunks::SetGetStatsFunction(get_stats_thunk);
  base::allocator::thunks::SetReleaseFreeMemoryFunction(
      release_free_memory_thunk);

  return 1;
}

// The CRT heap cleanup stub.
extern "C" void _heap_term() {}

// We set this to 1 because part of the CRT uses a check of _crtheap != 0
// to test whether the CRT has been initialized.  Once we've ripped out
// the allocators from libcmt, we need to provide this definition so that
// the rest of the CRT is still usable.
extern "C" void* _crtheap = reinterpret_cast<void*>(1);

// Provide support for aligned memory through Windows only _aligned_malloc().
void* _aligned_malloc(size_t size, size_t alignment) {
  // _aligned_malloc guarantees parameter validation, so do so here.  These
  // checks are somewhat stricter than _aligned_malloc() since we're effectively
  // using memalign() under the hood.
  DCHECK_GT(size, 0U);
  DCHECK_EQ(alignment & (alignment - 1), 0U);
  DCHECK_EQ(alignment % sizeof(void*), 0U);

  void* ptr;
  for (;;) {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
    switch (allocator) {
      case JEMALLOC:
        ptr = je_memalign(alignment, size);
        break;
      case WINHEAP:
      case WINLFH:
        ptr = win_heap_memalign(alignment, size);
        break;
      case TCMALLOC:
      default:
        ptr = tc_memalign(alignment, size);
        break;
    }
#else
    // TCMalloc case.
    ptr = tc_memalign(alignment, size);
#endif
    if (ptr) {
      // Sanity check alignment.
      DCHECK_EQ(reinterpret_cast<uintptr_t>(ptr) & (alignment - 1), 0U);
      return ptr;
    }

    if (!new_mode || !call_new_handler(true))
      break;
  }
  return ptr;
}

void _aligned_free(void* p) {
  // Both JEMalloc and TCMalloc return pointers from memalign() that are safe to
  // use with free().  Pointers allocated with win_heap_memalign() MUST be freed
  // via win_heap_memalign_free() since the aligned pointer is not the real one.
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  switch (allocator) {
    case JEMALLOC:
      je_free(p);
      return;
    case WINHEAP:
    case WINLFH:
      win_heap_memalign_free(p);
      return;
  }
#endif
  // TCMalloc case.
  do_free(p);
}

#endif  // WIN32

#include "generic_allocators.cc"

}  // extern C

namespace base {
namespace allocator {

void SetupSubprocessAllocator() {
#ifdef ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
  size_t primary_length = 0;
  getenv_s(&primary_length, NULL, 0, primary_name);

  size_t secondary_length = 0;
  char buffer[20];
  getenv_s(&secondary_length, buffer, sizeof(buffer), secondary_name);
  DCHECK_GT(sizeof(buffer), secondary_length);
  buffer[sizeof(buffer) - 1] = '\0';

  if (secondary_length || !primary_length) {
    // Don't use the environment variable if ADDRESS_SANITIZER is defined on
    // Windows, as the implementation require Winheap to be the allocator.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_WIN))
    const char* secondary_value = secondary_length ? buffer : "TCMALLOC";
    // Force renderer (or other subprocesses) to use secondary_value.
#else
    const char* secondary_value = "WINHEAP";
#endif
    int ret_val = _putenv_s(primary_name, secondary_value);
    DCHECK_EQ(0, ret_val);
  }
#endif  // ENABLE_DYNAMIC_ALLOCATOR_SWITCHING
}

void* TCMallocDoMallocForTest(size_t size) {
  return do_malloc(size);
}

void TCMallocDoFreeForTest(void* ptr) {
  do_free(ptr);
}

size_t ExcludeSpaceForMarkForTest(size_t size) {
  return ExcludeSpaceForMark(size);
}

}  // namespace allocator.
}  // namespace base.
