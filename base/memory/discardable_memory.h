// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace base {

enum LockDiscardableMemoryStatus {
  DISCARDABLE_MEMORY_FAILED = -1,
  DISCARDABLE_MEMORY_PURGED = 0,
  DISCARDABLE_MEMORY_SUCCESS = 1
};

// Platform abstraction for discardable memory. DiscardableMemory is used to
// cache large objects without worrying about blowing out memory, both on mobile
// devices where there is no swap, and desktop devices where unused free memory
// should be used to help the user experience. This is preferable to releasing
// memory in response to an OOM signal because it is simpler, though it has less
// flexibility as to which objects get discarded.
//
// Discardable memory has two states: locked and unlocked. While the memory is
// locked, it will not be discarded. Unlocking the memory allows the OS to
// reclaim it if needed. Locks do not nest.
//
// Notes:
//   - The paging behavior of memory while it is locked is not specified. While
//     mobile platforms will not swap it out, it may qualify for swapping
//     on desktop platforms. It is not expected that this will matter, as the
//     preferred pattern of usage for DiscardableMemory is to lock down the
//     memory, use it as quickly as possible, and then unlock it.
//   - Because of memory alignment, the amount of memory allocated can be
//     larger than the requested memory size. It is not very efficient for
//     small allocations.
//
// References:
//   - Linux: http://lwn.net/Articles/452035/
//   - Mac: http://trac.webkit.org/browser/trunk/Source/WebCore/platform/mac/PurgeableBufferMac.cpp
//          the comment starting with "vm_object_purgable_control" at
//            http://www.opensource.apple.com/source/xnu/xnu-792.13.8/osfmk/vm/vm_object.c
class BASE_EXPORT DiscardableMemory {
 public:
  DiscardableMemory();

  // If the discardable memory is locked, the destructor will unlock it.
  // The opened file will also be closed after this.
  ~DiscardableMemory();

  // Check whether the system supports discardable memory.
  static bool Supported();

  // Initialize the DiscardableMemory object. On success, this function returns
  // true and the memory is locked. This should only be called once.
  // This call could fail because of platform-specific limitations and the user
  // should stop using the DiscardableMemory afterwards.
  bool InitializeAndLock(size_t size);

  // Lock the memory so that it will not be purged by the system. Returns
  // DISCARDABLE_MEMORY_SUCCESS on success. If the return value is
  // DISCARDABLE_MEMORY_FAILED then this object should be discarded and
  // a new one should be created. If the return value is
  // DISCARDABLE_MEMORY_PURGED then the memory is present but any data that
  // was in it is gone.
  LockDiscardableMemoryStatus Lock() WARN_UNUSED_RESULT;

  // Unlock the memory so that it can be purged by the system. Must be called
  // after every successful lock call.
  void Unlock();

  // Return the memory address held by this object. The object must be locked
  // before calling this. Otherwise, this will cause a DCHECK error.
  void* Memory() const;

  // Testing utility calls.

  // Check whether a purge of all discardable memory in the system is supported.
  // Use only for testing!
  static bool PurgeForTestingSupported();

  // Purge all discardable memory in the system. This call has global effects
  // across all running processes, so it should only be used for testing!
  static void PurgeForTesting();

 private:
#if defined(OS_ANDROID)
  // Maps the discardable memory into the caller's address space.
  // Returns true on success, false otherwise.
  bool Map();

  // Unmaps the discardable memory from the caller's address space.
  void Unmap();

  // Reserve a file descriptor. When reaching the fd limit, this call returns
  // false and initialization should fail.
  bool ReserveFileDescriptor();

  // Release a file descriptor so that others can reserve it.
  void ReleaseFileDescriptor();
#endif  // OS_ANDROID

  void* memory_;
  size_t size_;
  bool is_locked_;
#if defined(OS_ANDROID)
  int fd_;
#endif  // OS_ANDROID

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemory);
};

}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_H_
