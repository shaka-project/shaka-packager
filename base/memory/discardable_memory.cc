// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/logging.h"

namespace base {

DiscardableMemory::DiscardableMemory()
    : memory_(NULL),
      size_(0),
      is_locked_(false)
#if defined(OS_ANDROID)
      , fd_(-1)
#endif  // OS_ANDROID
      {
  DCHECK(Supported());
}

void* DiscardableMemory::Memory() const {
  DCHECK(is_locked_);
  return memory_;
}

// Stub implementations for platforms that don't support discardable memory.

#if !defined(OS_ANDROID) && !defined(OS_MACOSX)

DiscardableMemory::~DiscardableMemory() {
  NOTIMPLEMENTED();
}

// static
bool DiscardableMemory::Supported() {
  return false;
}

bool DiscardableMemory::InitializeAndLock(size_t size) {
  NOTIMPLEMENTED();
  return false;
}

LockDiscardableMemoryStatus DiscardableMemory::Lock() {
  NOTIMPLEMENTED();
  return DISCARDABLE_MEMORY_FAILED;
}

void DiscardableMemory::Unlock() {
  NOTIMPLEMENTED();
}

// static
bool DiscardableMemory::PurgeForTestingSupported() {
  return false;
}

// static
void DiscardableMemory::PurgeForTesting() {
  NOTIMPLEMENTED();
}

#endif  // OS_*

}  // namespace base
