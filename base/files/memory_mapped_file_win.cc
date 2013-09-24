// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/memory_mapped_file.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/threading/thread_restrictions.h"

namespace base {

MemoryMappedFile::MemoryMappedFile()
    : file_(INVALID_HANDLE_VALUE),
      file_mapping_(INVALID_HANDLE_VALUE),
      data_(NULL),
      length_(INVALID_FILE_SIZE) {
}

bool MemoryMappedFile::InitializeAsImageSection(const FilePath& file_name) {
  if (IsValid())
    return false;
  file_ = CreatePlatformFile(file_name, PLATFORM_FILE_OPEN | PLATFORM_FILE_READ,
                             NULL, NULL);

  if (file_ == kInvalidPlatformFileValue) {
    DLOG(ERROR) << "Couldn't open " << file_name.AsUTF8Unsafe();
    return false;
  }

  if (!MapFileToMemoryInternalEx(SEC_IMAGE)) {
    CloseHandles();
    return false;
  }

  return true;
}

bool MemoryMappedFile::MapFileToMemoryInternal() {
  return MapFileToMemoryInternalEx(0);
}

bool MemoryMappedFile::MapFileToMemoryInternalEx(int flags) {
  ThreadRestrictions::AssertIOAllowed();

  if (file_ == INVALID_HANDLE_VALUE)
    return false;

  length_ = ::GetFileSize(file_, NULL);
  if (length_ == INVALID_FILE_SIZE)
    return false;

  file_mapping_ = ::CreateFileMapping(file_, NULL, PAGE_READONLY | flags,
                                      0, 0, NULL);
  if (!file_mapping_) {
    // According to msdn, system error codes are only reserved up to 15999.
    // http://msdn.microsoft.com/en-us/library/ms681381(v=VS.85).aspx.
    UMA_HISTOGRAM_ENUMERATION("MemoryMappedFile.CreateFileMapping",
                              logging::GetLastSystemErrorCode(), 16000);
    return false;
  }

  data_ = static_cast<uint8*>(
      ::MapViewOfFile(file_mapping_, FILE_MAP_READ, 0, 0, 0));
  if (!data_) {
    UMA_HISTOGRAM_ENUMERATION("MemoryMappedFile.MapViewOfFile",
                              logging::GetLastSystemErrorCode(), 16000);
  }
  return data_ != NULL;
}

void MemoryMappedFile::CloseHandles() {
  if (data_)
    ::UnmapViewOfFile(data_);
  if (file_mapping_ != INVALID_HANDLE_VALUE)
    ::CloseHandle(file_mapping_);
  if (file_ != INVALID_HANDLE_VALUE)
    ::CloseHandle(file_);

  data_ = NULL;
  file_mapping_ = file_ = INVALID_HANDLE_VALUE;
  length_ = INVALID_FILE_SIZE;
}

}  // namespace base
