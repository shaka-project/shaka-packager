// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/local_file.h"

#include <stdio.h>
#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)
#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"

namespace shaka {

// Always open files in binary mode.
const char kAdditionalFileMode[] = "b";

LocalFile::LocalFile(const char* file_name, const char* mode)
    : File(file_name), file_mode_(mode), internal_file_(NULL) {
  if (file_mode_.find(kAdditionalFileMode) == std::string::npos)
    file_mode_ += kAdditionalFileMode;
}

bool LocalFile::Close() {
  bool result = true;
  if (internal_file_) {
    result = base::CloseFile(internal_file_);
    internal_file_ = NULL;
  }
  delete this;
  return result;
}

int64_t LocalFile::Read(void* buffer, uint64_t length) {
  DCHECK(buffer != NULL);
  DCHECK(internal_file_ != NULL);
  size_t bytes_read = fread(buffer, sizeof(char), length, internal_file_);
  VLOG(2) << "Read " << length << " return " << bytes_read << " error "
          << ferror(internal_file_);
  if (bytes_read == 0 && ferror(internal_file_) != 0) {
    return -1;
  }
  return bytes_read;
}

int64_t LocalFile::Write(const void* buffer, uint64_t length) {
  DCHECK(buffer != NULL);
  DCHECK(internal_file_ != NULL);
  size_t bytes_written = fwrite(buffer, sizeof(char), length, internal_file_);
  VLOG(2) << "Write " << length << " return " << bytes_written << " error "
          << ferror(internal_file_);
  if (bytes_written == 0 && ferror(internal_file_) != 0) {
    return -1;
  }
  return bytes_written;
}

int64_t LocalFile::Size() {
  DCHECK(internal_file_ != NULL);

  // Flush any buffered data, so we get the true file size.
  if (!Flush()) {
    LOG(ERROR) << "Cannot flush file.";
    return -1;
  }

  int64_t file_size;
  if (!base::GetFileSize(base::FilePath::FromUTF8Unsafe(file_name()),
                         &file_size)) {
    LOG(ERROR) << "Cannot get file size.";
    return -1;
  }
  return file_size;
}

bool LocalFile::Flush() {
  DCHECK(internal_file_ != NULL);
  return ((fflush(internal_file_) == 0) && !ferror(internal_file_));
}

bool LocalFile::Seek(uint64_t position) {
#if defined(OS_WIN)
  return _fseeki64(internal_file_, static_cast<__int64>(position), SEEK_SET) ==
         0;
#else
  return fseeko(internal_file_, position, SEEK_SET) >= 0;
#endif  // !defined(OS_WIN)
}

bool LocalFile::Tell(uint64_t* position) {
#if defined(OS_WIN)
  __int64 offset = _ftelli64(internal_file_);
#else
  off_t offset = ftello(internal_file_);
#endif  // !defined(OS_WIN)
  if (offset < 0)
    return false;
  *position = static_cast<uint64_t>(offset);
  return true;
}

LocalFile::~LocalFile() {}

bool LocalFile::Open() {
  base::FilePath file_path(base::FilePath::FromUTF8Unsafe(file_name()));

  // Create upper level directories for write mode.
  if (file_mode_.find("w") != std::string::npos) {
    base::File::Error error;
    // The function returns success if the directories already exist.
    if (!base::CreateDirectoryAndGetError(file_path.DirName(), &error)) {
      LOG(ERROR) << "Failed to create directories for file '"
                 << file_path.AsUTF8Unsafe()
                 << "'. Error: " << base::File::ErrorToString(error);
      return false;
    }
  }

  internal_file_ = base::OpenFile(file_path, file_mode_.c_str());
  return (internal_file_ != NULL);
}

bool LocalFile::Delete(const char* file_name) {
  return base::DeleteFile(base::FilePath::FromUTF8Unsafe(file_name), false);
}

}  // namespace shaka
