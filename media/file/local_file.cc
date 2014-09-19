// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/file/local_file.h"

#include "base/file_util.h"
#include "base/logging.h"

namespace edash_packager {
namespace media {

LocalFile::LocalFile(const char* file_name, const char* mode)
    : File(file_name), file_mode_(mode), internal_file_(NULL) {}

bool LocalFile::Close() {
  bool result = true;
  if (internal_file_) {
    result = base::CloseFile(internal_file_);
    internal_file_ = NULL;
  }
  delete this;
  return result;
}

int64 LocalFile::Read(void* buffer, uint64 length) {
  DCHECK(buffer != NULL);
  DCHECK(internal_file_ != NULL);
  return fread(buffer, sizeof(char), length, internal_file_);
}

int64 LocalFile::Write(const void* buffer, uint64 length) {
  DCHECK(buffer != NULL);
  DCHECK(internal_file_ != NULL);
  return fwrite(buffer, sizeof(char), length, internal_file_);
}

int64 LocalFile::Size() {
  DCHECK(internal_file_ != NULL);

  // Flush any buffered data, so we get the true file size.
  if (!Flush()) {
    LOG(ERROR) << "Cannot flush file.";
    return -1;
  }

  int64 file_size;
  if (!base::GetFileSize(base::FilePath(file_name()), &file_size)) {
    LOG(ERROR) << "Cannot get file size.";
    return -1;
  }
  return file_size;
}

bool LocalFile::Flush() {
  DCHECK(internal_file_ != NULL);
  return ((fflush(internal_file_) == 0) && !ferror(internal_file_));
}

bool LocalFile::Eof() {
  DCHECK(internal_file_ != NULL);
  return static_cast<bool>(feof(internal_file_));
}

LocalFile::~LocalFile() {}

bool LocalFile::Open() {
  internal_file_ =
      base::OpenFile(base::FilePath(file_name()), file_mode_.c_str());
  return (internal_file_ != NULL);
}

}  // namespace media
}  // namespace edash_packager
