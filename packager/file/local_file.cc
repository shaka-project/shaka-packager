// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/local_file.h>

#if defined(OS_WIN)
#include <windows.h>
#else
#include <sys/stat.h>
#endif  // defined(OS_WIN)

#include <cstdio>
#include <filesystem>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>

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
    result = fclose(internal_file_) == 0;
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

void LocalFile::CloseForWriting() {}

int64_t LocalFile::Size() {
  DCHECK(internal_file_ != NULL);

  // Flush any buffered data, so we get the true file size.
  if (!Flush()) {
    LOG(ERROR) << "Cannot flush file.";
    return -1;
  }

  std::error_code ec;
  auto file_path = std::filesystem::u8path(file_name());
  int64_t file_size = std::filesystem::file_size(file_path, ec);
  if (ec) {
    LOG(ERROR) << "Cannot get file size, error: " << ec;
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
  auto file_path = std::filesystem::u8path(file_name());

  // Create upper level directories for write mode.
  if (file_mode_.find("w") != std::string::npos) {
    // From the return value of filesystem::create_directories, you can't tell
    // the difference between pre-existing directories and failure.  So check
    // first if it needs to be created.
    auto parent_path = file_path.parent_path();
    std::error_code ec;
    if (parent_path != "" && !std::filesystem::is_directory(parent_path, ec)) {
      if (!std::filesystem::create_directories(parent_path, ec)) {
        return false;
      }
    }
  }

  internal_file_ = fopen(file_path.u8string().c_str(), file_mode_.c_str());
  return (internal_file_ != NULL);
}

bool LocalFile::Delete(const char* file_name) {
  auto file_path = std::filesystem::u8path(file_name);
  std::error_code ec;
  // On error (ec truthy), remove() will return false anyway.
  return std::filesystem::remove(file_path, ec);
}

}  // namespace shaka
