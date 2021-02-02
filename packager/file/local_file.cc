// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/local_file.h"

#include <stdio.h>
#if defined(OS_WIN)
#include <windows.h>
#else
#include <sys/stat.h>
#endif  // defined(OS_WIN)
#include "packager/base/files/file_path.h"
#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"

namespace shaka {
namespace {

// Check if the directory |path| exists. Returns false if it does not exist or
// it is not a directory. On non-Windows, |mode| will be filled with the file
// permission bits on success.
bool DirectoryExists(const base::FilePath& path, int* mode) {
#if defined(OS_WIN)
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES)
    return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
  struct stat info;
  if (stat(path.value().c_str(), &info) != 0)
    return false;
  if (S_ISDIR(info.st_mode)) {
    const int FILE_PERMISSION_MASK = S_IRWXU | S_IRWXG | S_IRWXO;
    if (mode)
      *mode = info.st_mode & FILE_PERMISSION_MASK;
    return true;
  }
#endif
  return false;
}

// Create all the inexistent directories in the path. Returns true on success or
// if the directory already exists.
bool CreateDirectory(const base::FilePath& full_path) {
  std::vector<base::FilePath> subpaths;

  // Collect a list of all parent directories.
  base::FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (base::FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // For non-Windows only. File permission for the new directories.
  // The file permission will be inherited from the last existing directory in
  // the file path. If none of the directory exists in the path, it is set to
  // 0755 by default.
  int mode = 0755;

  // Iterate through the parents and create the missing ones.
  for (auto i = subpaths.rbegin(); i != subpaths.rend(); ++i) {
    if (DirectoryExists(*i, &mode)) {
      continue;
    }
#if defined(OS_WIN)
    if (::CreateDirectory(i->value().c_str(), nullptr)) {
      continue;
    }
#else
    if (mkdir(i->value().c_str(), mode) == 0) {
      continue;
    }
#endif

    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    const auto saved_error_code = ::logging::GetLastSystemErrorCode();
    if (!DirectoryExists(*i, nullptr)) {
      LOG(ERROR) << "Failed to create directory " << i->value().c_str()
                 << " ErrorCode " << saved_error_code;
      return false;
    }
  }
  return true;
}

}  // namespace

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
    // The function returns true if the directories already exist.
    if (!shaka::CreateDirectory(file_path.DirName())) {
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
