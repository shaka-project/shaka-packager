// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/memory_mapped_file.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace base {

MemoryMappedFile::~MemoryMappedFile() {
  CloseHandles();
}

bool MemoryMappedFile::Initialize(const FilePath& file_name) {
  if (IsValid())
    return false;

  if (!MapFileToMemory(file_name)) {
    CloseHandles();
    return false;
  }

  return true;
}

bool MemoryMappedFile::Initialize(PlatformFile file) {
  if (IsValid())
    return false;

  file_ = file;

  if (!MapFileToMemoryInternal()) {
    CloseHandles();
    return false;
  }

  return true;
}

bool MemoryMappedFile::IsValid() const {
  return data_ != NULL;
}

bool MemoryMappedFile::MapFileToMemory(const FilePath& file_name) {
  file_ = CreatePlatformFile(file_name, PLATFORM_FILE_OPEN | PLATFORM_FILE_READ,
                             NULL, NULL);

  if (file_ == kInvalidPlatformFileValue) {
    DLOG(ERROR) << "Couldn't open " << file_name.AsUTF8Unsafe();
    return false;
  }

  return MapFileToMemoryInternal();
}

}  // namespace base
