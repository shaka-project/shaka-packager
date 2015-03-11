// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/file.h"

#include "packager/base/logging.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/file/local_file.h"
#include "packager/media/file/udp_file.h"
#include "packager/base/strings/string_util.h"

namespace edash_packager {
namespace media {

const char* kLocalFilePrefix = "file://";
const char* kUdpFilePrefix = "udp://";

namespace {

typedef File* (*FileFactoryFunction)(const char* file_name, const char* mode);
typedef bool (*FileDeleteFunction)(const char* file_name);

struct SupportedTypeInfo {
  const char* type;
  size_t type_length;
  const FileFactoryFunction factory_function;
  const FileDeleteFunction delete_function;
};

File* CreateLocalFile(const char* file_name, const char* mode) {
  return new LocalFile(file_name, mode);
}

bool DeleteLocalFile(const char* file_name) {
  return LocalFile::Delete(file_name);
}

File* CreateUdpFile(const char* file_name, const char* mode) {
  if (base::strcasecmp(mode, "r")) {
    NOTIMPLEMENTED() << "UdpFile only supports read (receive) mode.";
    return NULL;
  }
  return new UdpFile(file_name);
}

static const SupportedTypeInfo kSupportedTypeInfo[] = {
  {
    kLocalFilePrefix,
    strlen(kLocalFilePrefix),
    &CreateLocalFile,
    &DeleteLocalFile
  },
  {
    kUdpFilePrefix,
    strlen(kUdpFilePrefix),
    &CreateUdpFile,
    NULL
  },
};

}  // namespace

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

bool File::Delete(const char* file_name) {
  for (size_t i = 0; i < arraysize(kSupportedTypeInfo); ++i) {
    const SupportedTypeInfo& type_info = kSupportedTypeInfo[i];
    if (strncmp(type_info.type, file_name, type_info.type_length) == 0) {
      return type_info.delete_function ?
          type_info.delete_function(file_name + type_info.type_length) :
          false;
    }
  }
  // Otherwise we assume it is a local file
  return DeleteLocalFile(file_name);
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
