// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Static class for hooking Win32 API routines.  For now,
// we only add one watcher at a time.
//
// TODO(mbelshe):  Support multiple watchers.

#ifndef MEMORY_WATCHER_MEMORY_HOOK_
#define MEMORY_WATCHER_MEMORY_HOOK_

#include "base/logging.h"

// When allocating memory for internal use with the MemoryHook,
// we must always use the MemoryHook's heap; otherwise, the memory
// gets tracked, and it becomes an infinite loop (allocation() calls
// MemoryHook() which calls allocation(), etc).
//
// PrivateHookAllocator is an STL-friendly Allocator so that STL lists,
// maps, etc can be used on the global MemoryHook's heap.
template <class T>
class PrivateHookAllocator {
 public:
  // These type definitions are needed for stl allocators.
  typedef size_t    size_type;
  typedef ptrdiff_t difference_type;
  typedef T*        pointer;
  typedef const T*  const_pointer;
  typedef T&        reference;
  typedef const T&  const_reference;
  typedef T         value_type;

  PrivateHookAllocator() {}

  // Allocate memory for STL.
  pointer allocate(size_type n, const void * = 0) {
    return reinterpret_cast<T*>(MemoryHook::Alloc(n * sizeof(T)));
  }

  // Deallocate memory for STL.
  void deallocate(void* p, size_type) {
    if (p)
      MemoryHook::Free(p);
  }

  // Construct the object
  void construct(pointer p, const T& val) {
    new (reinterpret_cast<T*>(p))T(val);
  }

  // Destruct an object
  void destroy(pointer p) { p->~T(); }

  size_type max_size() const { return size_t(-1); }

  template <class U>
  struct rebind { typedef PrivateHookAllocator<U> other; };

  template <class U>
  PrivateHookAllocator(const PrivateHookAllocator<U>&) {}
};

template<class T, class U> inline
bool operator==(const PrivateHookAllocator<T>&,
                const PrivateHookAllocator<U>&) {
  return (true);
}

template<class T, class U> inline
bool operator!=(const PrivateHookAllocator<T>& left,
                const PrivateHookAllocator<U>& right) {
  return (!(left == right));
}


// Classes which monitor memory from these hooks implement
// the MemoryObserver interface.
class MemoryObserver {
 public:
  virtual ~MemoryObserver() {}

  // Track a pointer.  Will capture the current StackTrace.
  virtual void OnTrack(HANDLE heap, int32 id, int32 size) = 0;

  // Untrack a pointer, removing it from our list.
  virtual void OnUntrack(HANDLE heap, int32 id, int32 size) = 0;
};

class MemoryHook : MemoryObserver {
 public:
  // Initialize the MemoryHook.  Must be called before
  // registering watchers.  This can be called repeatedly,
  // but is not thread safe.
  static bool Initialize();

  // Returns true is memory allocations and deallocations
  // are being traced.
  static bool hooked() { return hooked_ != NULL; }

  // Register a class to receive memory allocation & deallocation
  // callbacks.  If we haven't hooked memory yet, this call will
  // force memory hooking to start.
  static bool RegisterWatcher(MemoryObserver* watcher);

  // Register a class to stop receiving callbacks.  If there are
  // no more watchers, this call will unhook memory.
  static bool UnregisterWatcher(MemoryObserver* watcher);

  // MemoryHook provides a private heap for allocating
  // unwatched memory.
  static void* Alloc(size_t size) {
    DCHECK(global_hook_ && global_hook_->heap_);
    return HeapAlloc(global_hook_->heap_, 0, size);
  }
  static void Free(void* ptr) {
    DCHECK(global_hook_ && global_hook_->heap_);
    HeapFree(global_hook_->heap_, 0, ptr);
  }

  // Access the global hook.  For internal use only from static "C"
  // hooks.
  static MemoryHook* hook() { return global_hook_; }

  // MemoryObserver interface.
  virtual void OnTrack(HANDLE hHeap, int32 id, int32 size);
  virtual void OnUntrack(HANDLE hHeap, int32 id, int32 size);

 private:
  MemoryHook();
  ~MemoryHook();

  // Enable memory tracing.  When memory is 'hooked',
  // MemoryWatchers which have registered will be called
  // as memory is allocated and deallocated.
  static bool Hook();

  // Disables memory tracing.
  static bool Unhook();

  // Create our private heap
  bool CreateHeap();

  // Close our private heap.
  bool CloseHeap();

  MemoryObserver* watcher_;
  HANDLE heap_;   // An internal accounting heap.
  static bool hooked_;
  static MemoryHook* global_hook_;
};

#endif  // MEMORY_WATCHER_MEMORY_HOOK_
