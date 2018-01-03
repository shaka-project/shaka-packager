// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/memory_file.h"

#include <string.h>  // for memcpy

#include <algorithm>
#include <map>
#include <memory>

#include "packager/base/logging.h"

namespace shaka {
namespace {

// A helper filesystem object.  This holds the data for the memory files.
class FileSystem {
 public:
  ~FileSystem() {}

  static FileSystem* Instance() {
    if (!g_file_system_)
      g_file_system_.reset(new FileSystem());

    return g_file_system_.get();
  }

  bool Exists(const std::string& file_name) const {
    return files_.find(file_name) != files_.end();
  }

  std::vector<uint8_t>* GetFile(const std::string& file_name) {
    return &files_[file_name];
  }

  void Delete(const std::string& file_name) { files_.erase(file_name); }

  void DeleteAll() { files_.clear(); }

 private:
  FileSystem() {}

  static std::unique_ptr<FileSystem> g_file_system_;

  std::map<std::string, std::vector<uint8_t> > files_;
  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

std::unique_ptr<FileSystem> FileSystem::g_file_system_;

}  // namespace

MemoryFile::MemoryFile(const std::string& file_name, const std::string& mode)
    : File(file_name), mode_(mode), file_(NULL), position_(0) {}

MemoryFile::~MemoryFile() {}

bool MemoryFile::Close() {
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
  FileSystem* file_system = FileSystem::Instance();
  if (mode_ == "r") {
    if (!file_system->Exists(file_name()))
      return false;
  } else if (mode_ == "w") {
    file_system->Delete(file_name());
  } else {
    NOTIMPLEMENTED() << "File mode " << mode_ << " not supported by MemoryFile";
    return false;
  }

  file_ = file_system->GetFile(file_name());
  DCHECK(file_);
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
