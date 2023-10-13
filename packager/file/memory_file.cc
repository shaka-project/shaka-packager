// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/memory_file.h>

#include <algorithm>
#include <cstring>  // for memcpy
#include <map>
#include <memory>
#include <set>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/synchronization/mutex.h>

#include <packager/macros/logging.h>

namespace shaka {
namespace {

// A helper filesystem object.  This holds the data for the memory files.
class FileSystem {
 public:
  ~FileSystem() {}

  static FileSystem* Instance() {
    static FileSystem instance;
    return &instance;
  }

  void Delete(const std::string& file_name) {
    absl::MutexLock auto_lock(&mutex_);

    if (open_files_.find(file_name) != open_files_.end()) {
      LOG(ERROR) << "File '" << file_name
                 << "' is still open. Deleting an open MemoryFile is not "
                    "allowed. Exit without deleting the file.";
      return;
    }

    files_.erase(file_name);
  }

  void DeleteAll() {
    absl::MutexLock auto_lock(&mutex_);
    if (!open_files_.empty()) {
      LOG(ERROR) << "There are still files open. Deleting an open MemoryFile "
                    "is not allowed. Exit without deleting the file.";
      return;
    }
    files_.clear();
  }

  std::vector<uint8_t>* Open(const std::string& file_name,
                             const std::string& mode) {
    absl::MutexLock auto_lock(&mutex_);

    if (open_files_.find(file_name) != open_files_.end()) {
      NOTIMPLEMENTED() << "File '" << file_name
                       << "' is already open. MemoryFile does not support "
                          "opening the same file before it is closed.";
      return nullptr;
    }

    auto iter = files_.find(file_name);
    if (mode == "r") {
      if (iter == files_.end())
        return nullptr;
    } else if (mode == "w") {
      if (iter != files_.end())
        iter->second.clear();
    } else {
      NOTIMPLEMENTED() << "File mode '" << mode
                       << "' not supported by MemoryFile";
      return nullptr;
    }

    open_files_[file_name] = mode;
    return &files_[file_name];
  }

  bool Close(const std::string& file_name) {
    absl::MutexLock auto_lock(&mutex_);

    auto iter = open_files_.find(file_name);
    if (iter == open_files_.end()) {
      LOG(ERROR) << "Cannot close file '" << file_name
                 << "' which is not open.";
      return false;
    }

    open_files_.erase(iter);
    return true;
  }

 private:
  FileSystem(const FileSystem&) = delete;
  FileSystem& operator=(const FileSystem&) = delete;

  FileSystem() = default;

  // Filename to file data map.
  std::map<std::string, std::vector<uint8_t>> files_ ABSL_GUARDED_BY(mutex_);
  // Filename to file open modes map.
  std::map<std::string, std::string> open_files_ ABSL_GUARDED_BY(mutex_);

  absl::Mutex mutex_;
};

}  // namespace

MemoryFile::MemoryFile(const std::string& file_name, const std::string& mode)
    : File(file_name), mode_(mode), file_(NULL), position_(0) {}

MemoryFile::~MemoryFile() {}

bool MemoryFile::Close() {
  if (!FileSystem::Instance()->Close(file_name()))
    return false;
  delete this;
  return true;
}

int64_t MemoryFile::Read(void* buffer, uint64_t length) {
  const uint64_t size = Size();
  DCHECK_LE(position_, size);
  if (position_ >= size)
    return 0;

  const uint64_t bytes_to_read = std::min(length, size - position_);
  memcpy(buffer, &(*file_)[position_], bytes_to_read);
  position_ += bytes_to_read;
  return bytes_to_read;
}

int64_t MemoryFile::Write(const void* buffer, uint64_t length) {
  // If length is zero, we won't resize the buffer and it is possible for
  // |position| to equal the length of the buffer. This will cause a segfault
  // when indexing into the buffer for the memcpy.
  if (length == 0) {
    return 0;
  }

  const uint64_t size = Size();
  if (size < position_ + length) {
    file_->resize(position_ + length);
  }

  memcpy(&(*file_)[position_], buffer, length);
  position_ += length;
  return length;
}

void MemoryFile::CloseForWriting() {}

int64_t MemoryFile::Size() {
  DCHECK(file_);
  return file_->size();
}

bool MemoryFile::Flush() {
  return true;
}

bool MemoryFile::Seek(uint64_t position) {
  if (Size() < static_cast<int64_t>(position))
    return false;

  position_ = position;
  return true;
}

bool MemoryFile::Tell(uint64_t* position) {
  *position = position_;
  return true;
}

bool MemoryFile::Open() {
  file_ = FileSystem::Instance()->Open(file_name(), mode_);
  if (!file_)
    return false;

  position_ = 0;
  return true;
}

void MemoryFile::DeleteAll() {
  FileSystem::Instance()->DeleteAll();
}

void MemoryFile::Delete(const std::string& file_name) {
  FileSystem::Instance()->Delete(file_name);
}

}  // namespace shaka
