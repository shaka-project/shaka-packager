// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <mach/mach.h>

#include "base/logging.h"

namespace base {

namespace {

// The VM subsystem allows tagging of memory and 240-255 is reserved for
// application use (see mach/vm_statistics.h). Pick 252 (after chromium's atomic
// weight of ~52).
const int kDiscardableMemoryTag = VM_MAKE_TAG(252);

}  // namespace

// static
bool DiscardableMemory::Supported() {
  return true;
}

DiscardableMemory::~DiscardableMemory() {
  if (memory_) {
    vm_deallocate(mach_task_self(),
                  reinterpret_cast<vm_address_t>(memory_),
                  size_);
  }
}

bool DiscardableMemory::InitializeAndLock(size_t size) {
  DCHECK(!memory_);
  size_ = size;

  vm_address_t buffer = 0;
  kern_return_t ret = vm_allocate(mach_task_self(),
                                  &buffer,
                                  size,
                                  VM_FLAGS_PURGABLE |
                                  VM_FLAGS_ANYWHERE |
                                  kDiscardableMemoryTag);

  if (ret != KERN_SUCCESS) {
    DLOG(ERROR) << "vm_allocate() failed";
    return false;
  }

  is_locked_ = true;
  memory_ = reinterpret_cast<void*>(buffer);
  return true;
}

LockDiscardableMemoryStatus DiscardableMemory::Lock() {
  DCHECK(!is_locked_);

  int state = VM_PURGABLE_NONVOLATILE;
  kern_return_t ret = vm_purgable_control(
      mach_task_self(),
      reinterpret_cast<vm_address_t>(memory_),
      VM_PURGABLE_SET_STATE,
      &state);

  if (ret != KERN_SUCCESS)
    return DISCARDABLE_MEMORY_FAILED;

  is_locked_ = true;
  return state & VM_PURGABLE_EMPTY ? DISCARDABLE_MEMORY_PURGED
                                   : DISCARDABLE_MEMORY_SUCCESS;
}

void DiscardableMemory::Unlock() {
  DCHECK(is_locked_);

  int state = VM_PURGABLE_VOLATILE | VM_VOLATILE_GROUP_DEFAULT;
  kern_return_t ret = vm_purgable_control(
      mach_task_self(),
      reinterpret_cast<vm_address_t>(memory_),
      VM_PURGABLE_SET_STATE,
      &state);

  if (ret != KERN_SUCCESS)
    DLOG(ERROR) << "Failed to unlock memory.";

  is_locked_ = false;
}

// static
bool DiscardableMemory::PurgeForTestingSupported() {
  return true;
}

// static
void DiscardableMemory::PurgeForTesting() {
  int state = 0;
  vm_purgable_control(mach_task_self(), 0, VM_PURGABLE_PURGE_ALL, &state);
}

}  // namespace base
