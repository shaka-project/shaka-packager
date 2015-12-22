// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/memory_file.h"

#include <string.h>  // for memcpy

#include <map>

#include "packager/base/logging.h"
#include "packager/base/memory/scoped_ptr.h"

namespace edash_packager {
namespace media {
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

  std::vector<uint8_t>* GetFile(const std::string& file_name) {
    return &files_[file_name];
  }

  void Delete(const std::string& file_name) { files_.erase(file_name); }

  void DeleteAll() { files_.clear(); }

 private:
  FileSystem() {}

  static scoped_ptr<FileSystem> g_file_system_;

  std::map<std::string, std::vector<uint8_t> > files_;
  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

scoped_ptr<FileSystem> FileSystem::g_file_system_;

}  // namespace

MemoryFile::MemoryFile(const std::string& file_name)
    : File(file_name),
      file_(FileSystem::Instance()->GetFile(file_name)),
      position_(0) {}

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
  const uint64_t size = Size();
  if (size < position_ + length) {
    file_->resize(position_ + length);
  }

  memcpy(&(*file_)[position_], buffer, length);
  position_ += length;
  return length;
}

int64_t MemoryFile::Size() {
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
  position_ = 0;
  return true;
}

void MemoryFile::DeleteAll() {
  FileSystem::Instance()->DeleteAll();
}

void MemoryFile::Delete(const std::string& file_name) {
  FileSystem::Instance()->Delete(file_name);
}

}  // namespace media
}  // namespace edash_packager

