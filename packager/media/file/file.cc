// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/file/file.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/file/local_file.h"
#include "media/file/udp_file.h"
#include "base/strings/string_util.h"

namespace edash_packager {
namespace media {

const char* kLocalFilePrefix = "file://";
const char* kUdpFilePrefix = "udp://";

typedef File* (*FileFactoryFunction)(const char* file_name, const char* mode);

struct SupportedTypeInfo {
  const char* type;
  size_t type_length;
  const FileFactoryFunction factory_function;
};

static File* CreateLocalFile(const char* file_name, const char* mode) {
  return new LocalFile(file_name, mode);
}

static File* CreateUdpFile(const char* file_name, const char* mode) {
  if (base::strcasecmp(mode, "r")) {
    NOTIMPLEMENTED() << "UdpFile only supports read (receive) mode.";
    return NULL;
  }
  return new UdpFile(file_name);
}

static const SupportedTypeInfo kSupportedTypeInfo[] = {
    { kLocalFilePrefix, strlen(kLocalFilePrefix), &CreateLocalFile },
    { kUdpFilePrefix, strlen(kUdpFilePrefix), &CreateUdpFile },
};

File* File::Create(const char* file_name, const char* mode) {
  for (size_t i = 0; i < arraysize(kSupportedTypeInfo); ++i) {
    const SupportedTypeInfo& type_info = kSupportedTypeInfo[i];
    if (strncmp(type_info.type, file_name, type_info.type_length) == 0) {
      return type_info.factory_function(file_name + type_info.type_length,
                                        mode);
    }
  }
  // Otherwise we assume it is a local file
  return CreateLocalFile(file_name, mode);
}

File* File::Open(const char* file_name, const char* mode) {
  File* file = File::Create(file_name, mode);
  if (!file)
    return NULL;
  if (!file->Open()) {
    delete file;
    return NULL;
  }
  return file;
}

int64_t File::GetFileSize(const char* file_name) {
  File* file = File::Open(file_name, "r");
  if (!file)
    return -1;
  int64_t res = file->Size();
  file->Close();
  return res;
}

bool File::ReadFileToString(const char* file_name, std::string* contents) {
  DCHECK(contents);

  File* file = File::Open(file_name, "r");
  if (!file)
    return false;

  const size_t kBufferSize = 0x40000;  // 256KB.
  scoped_ptr<char[]> buf(new char[kBufferSize]);

  int64_t len;
  while ((len = file->Read(buf.get(), kBufferSize)) > 0)
    contents->append(buf.get(), len);

  file->Close();
  return len == 0;
}

}  // namespace media
}  // namespace edash_packager
