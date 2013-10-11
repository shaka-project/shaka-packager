// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license tha can be
// found in the LICENSE file.

#include "media/file/file.h"

#include "media/file/local_file.h"

namespace media {

const char* kLocalFilePrefix = "file://";

typedef File* (*FileFactoryFunction)(const char* fname, const char* mode);

struct SupportedTypeInfo {
  const char* type;
  int type_length;
  const FileFactoryFunction factory_function;
};

static File* CreateLocalFile(const char* fname, const char* mode) {
  return new LocalFile(fname, mode);
}

static const SupportedTypeInfo kSupportedTypeInfo[] = {
    { kLocalFilePrefix, strlen(kLocalFilePrefix), &CreateLocalFile },
};

File* File::Create(const char* fname, const char* mode) {
  for (size_t i = 0; i < arraysize(kSupportedTypeInfo); ++i) {
    const SupportedTypeInfo& type_info = kSupportedTypeInfo[i];
    if (strncmp(type_info.type, fname, type_info.type_length) == 0) {
      return type_info.factory_function(fname + type_info.type_length, mode);
    }
  }
  // Otherwise we assume it is a local file
  return CreateLocalFile(fname, mode);
}

File* File::Open(const char* name, const char* mode) {
  File* file = File::Create(name, mode);
  if (!file) {
    return NULL;
  }
  if (!file->Open()) {
    delete file;
    return NULL;
  }
  return file;
}

// Return the file size or -1 on failure.
// Requires opening and closing the file.
int64 File::GetFileSize(const char* name) {
  File* f = File::Open(name, "r");
  if (!f) {
    return -1;
  }
  int64 res = f->Size();
  f->Close();
  return res;
}

}  // namespace media
