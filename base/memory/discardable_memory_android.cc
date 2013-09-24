// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <sys/mman.h>
#include <unistd.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/lock.h"
#include "third_party/ashmem/ashmem.h"

namespace {

base::LazyInstance<base::Lock>::Leaky g_discardable_memory_lock =
    LAZY_INSTANCE_INITIALIZER;

// Total number of discardable memory in the process.
int g_num_discardable_memory = 0;

// Upper limit on the number of discardable memory to avoid hitting file
// descriptor limit.
const int kDiscardableMemoryNumLimit = 128;

}

namespace base {

// static
bool DiscardableMemory::Supported() {
  return true;
}

DiscardableMemory::~DiscardableMemory() {
  if (is_locked_)
    Unlock();
  // If fd_ is smaller than 0, initialization must have failed and
  // g_num_discardable_memory is not incremented by the caller.
  if (fd_ < 0)
    return;
  HANDLE_EINTR(close(fd_));
  fd_ = -1;
  ReleaseFileDescriptor();
}

bool DiscardableMemory::ReserveFileDescriptor() {
  base::AutoLock lock(g_discardable_memory_lock.Get());
  if (g_num_discardable_memory < kDiscardableMemoryNumLimit) {
    ++g_num_discardable_memory;
    return true;
  }
  return false;
}

void DiscardableMemory::ReleaseFileDescriptor() {
  base::AutoLock lock(g_discardable_memory_lock.Get());
  --g_num_discardable_memory;
  DCHECK_LE(0, g_num_discardable_memory);
}

bool DiscardableMemory::InitializeAndLock(size_t size) {
  // When this function returns true, fd_ should be larger or equal than 0
  // and g_num_discardable_memory is incremented by 1. Otherwise, fd_
  // is less than 0 and g_num_discardable_memory is not incremented by
  // the caller.
  DCHECK_EQ(fd_, -1);
  DCHECK(!memory_);
  if (!ReserveFileDescriptor())
    return false;

  size_ = size;
  fd_ = ashmem_create_region("", size);

  if (fd_ < 0) {
    DLOG(ERROR) << "ashmem_create_region() failed";
    ReleaseFileDescriptor();
    return false;
  }

  int err = ashmem_set_prot_region(fd_, PROT_READ | PROT_WRITE);
  if (err < 0) {
    DLOG(ERROR) << "Error " << err << " when setting protection of ashmem";
    HANDLE_EINTR(close(fd_));
    fd_ = -1;
    ReleaseFileDescriptor();
    return false;
  }

  if (!Map()) {
    // Close the file descriptor in case of any initialization errors.
    HANDLE_EINTR(close(fd_));
    fd_ = -1;
    ReleaseFileDescriptor();
    return false;
  }

  is_locked_ = true;
  return true;
}

LockDiscardableMemoryStatus DiscardableMemory::Lock() {
  DCHECK_NE(fd_, -1);
  DCHECK(!is_locked_);

  bool purged = false;
  if (ashmem_pin_region(fd_, 0, 0) == ASHMEM_WAS_PURGED)
    purged = true;

  if (!Map())
    return DISCARDABLE_MEMORY_FAILED;

  is_locked_ = true;
  return purged ? DISCARDABLE_MEMORY_PURGED : DISCARDABLE_MEMORY_SUCCESS;
}

void DiscardableMemory::Unlock() {
  DCHECK_GE(fd_, 0);
  DCHECK(is_locked_);

  Unmap();
  if (ashmem_unpin_region(fd_, 0, 0))
    DLOG(ERROR) << "Failed to unpin memory.";
  is_locked_ = false;
}

bool DiscardableMemory::Map() {
  DCHECK(!memory_);
  // There is a problem using MAP_PRIVATE here. As we are constantly calling
  // Lock() and Unlock(), data could get lost if they are not written to the
  // underlying file when Unlock() gets called.
  memory_ = mmap(NULL, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (memory_ == (void*)-1) {
    DPLOG(ERROR) << "Failed to map memory.";
    memory_ = NULL;
    if (ashmem_unpin_region(fd_, 0, 0))
      DLOG(ERROR) << "Failed to unpin memory.";
    return false;
  }
  return true;
}

void DiscardableMemory::Unmap() {
  DCHECK(memory_);

  if (-1 == munmap(memory_, size_))
    DPLOG(ERROR) << "Failed to unmap memory.";

  memory_ = NULL;
}

// static
bool DiscardableMemory::PurgeForTestingSupported() {
  return false;
}

// static
void DiscardableMemory::PurgeForTesting() {
  NOTIMPLEMENTED();
}

}  // namespace base
