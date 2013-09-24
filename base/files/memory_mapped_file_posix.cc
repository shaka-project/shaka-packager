// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/memory_mapped_file.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"

namespace base {

MemoryMappedFile::MemoryMappedFile()
    : file_(kInvalidPlatformFileValue),
      data_(NULL),
      length_(0) {
}

bool MemoryMappedFile::MapFileToMemoryInternal() {
  ThreadRestrictions::AssertIOAllowed();

  struct stat file_stat;
  if (fstat(file_, &file_stat) == kInvalidPlatformFileValue) {
    DLOG(ERROR) << "Couldn't fstat " << file_ << ", errno " << errno;
    return false;
  }
  length_ = file_stat.st_size;

  data_ = static_cast<uint8*>(
      mmap(NULL, length_, PROT_READ, MAP_SHARED, file_, 0));
  if (data_ == MAP_FAILED)
    DLOG(ERROR) << "Couldn't mmap " << file_ << ", errno " << errno;

  return data_ != MAP_FAILED;
}

void MemoryMappedFile::CloseHandles() {
  ThreadRestrictions::AssertIOAllowed();

  if (data_ != NULL)
    munmap(data_, length_);
  if (file_ != kInvalidPlatformFileValue)
    ignore_result(HANDLE_EINTR(close(file_)));

  data_ = NULL;
  length_ = 0;
  file_ = kInvalidPlatformFileValue;
}

}  // namespace base
