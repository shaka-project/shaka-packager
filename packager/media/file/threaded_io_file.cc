// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/threaded_io_file.h"

#include "packager/base/bind.h"
#include "packager/base/bind_helpers.h"
#include "packager/base/threading/platform_thread.h"
#include "packager/media/base/closure_thread.h"

namespace edash_packager {
namespace media {

using base::subtle::NoBarrier_Load;
using base::subtle::NoBarrier_Store;

ThreadedIoFile::ThreadedIoFile(scoped_ptr<File, FileCloser> internal_file,
                               Mode mode,
                               uint64_t io_cache_size,
                               uint64_t io_block_size)
    : File(internal_file->file_name()),
      internal_file_(internal_file.Pass()),
      mode_(mode),
      cache_(io_cache_size),
      io_buffer_(io_block_size),
      position_(0),
      size_(0),
      eof_(false),
      flushing_(false),
      flush_complete_event_(false, false),
      internal_file_error_(0){
  DCHECK(internal_file_);
}

ThreadedIoFile::~ThreadedIoFile() {}

bool ThreadedIoFile::Open() {
  DCHECK(internal_file_);

  if (!internal_file_->Open())
    return false;

  position_ = 0;
  size_ = internal_file_->Size();

  thread_.reset(new ClosureThread("ThreadedIoFile",
                                  base::Bind(mode_ == kInputMode ?
                                             &ThreadedIoFile::RunInInputMode :
                                             &ThreadedIoFile::RunInOutputMode,
                                             base::Unretained(this))));
  thread_->Start();
  return true;
}

bool ThreadedIoFile::Close() {
  DCHECK(internal_file_);
  DCHECK(thread_);

  if (mode_ == kOutputMode)
    Flush();

  cache_.Close();
  thread_->Join();

  bool result = internal_file_.release()->Close();
  delete this;
  return result;
}

int64_t ThreadedIoFile::Read(void* buffer, uint64_t length) {
  DCHECK(internal_file_);
  DCHECK(thread_);
  DCHECK_EQ(kInputMode, mode_);

  if (NoBarrier_Load(&eof_) && !cache_.BytesCached())
    return 0;

  if (NoBarrier_Load(&internal_file_error_))
    return NoBarrier_Load(&internal_file_error_);


  uint64_t bytes_read = cache_.Read(buffer, length);
  position_ += bytes_read;

  return bytes_read;
}

int64_t ThreadedIoFile::Write(const void* buffer, uint64_t length) {
  DCHECK(internal_file_);
  DCHECK(thread_);
  DCHECK_EQ(kOutputMode, mode_);

  if (NoBarrier_Load(&internal_file_error_))
    return NoBarrier_Load(&internal_file_error_);

  uint64_t bytes_written = cache_.Write(buffer, length);
  position_ += bytes_written;
  if (position_ > size_)
    size_ = position_;

  return bytes_written;
}

int64_t ThreadedIoFile::Size() {
  DCHECK(internal_file_);
  DCHECK(thread_);

  return size_;
}

bool ThreadedIoFile::Flush() {
  DCHECK(internal_file_);
  DCHECK(thread_);
  DCHECK_EQ(kOutputMode, mode_);

  flushing_ = true;
  cache_.Close();
  flush_complete_event_.Wait();
  return internal_file_->Flush();
}

bool ThreadedIoFile::Seek(uint64_t position) {
  if (mode_ == kOutputMode) {
    // Writing. Just flush the cache and seek.
    if (!Flush()) return false;
    if (!internal_file_->Seek(position)) return false;
  } else {
    // Reading. Close cache, wait for I/O thread to exit, seek, and restart
    // I/O thread.
    cache_.Close();
    thread_->Join();
    bool result = internal_file_->Seek(position);
    if (!result) {
      // Seek failed. Seek to logical position instead.
      if (!internal_file_->Seek(position_) && (position != position_)) {
        LOG(WARNING) << "Seek failed. ThreadedIoFile left in invalid state.";
      }
    }
    cache_.Reopen();
    eof_ = false;
    thread_.reset(new ClosureThread("ThreadedIoFile",
                                    base::Bind(&ThreadedIoFile::RunInInputMode,
                                               base::Unretained(this))));
    thread_->Start();
    if (!result) return false;
  }
  position_ = position;
  return true;
}

bool ThreadedIoFile::Tell(uint64_t* position) {
  DCHECK(position);

  *position = position_;
  return true;
}

void ThreadedIoFile::RunInInputMode() {
  DCHECK(internal_file_);
  DCHECK(thread_);
  DCHECK_EQ(kInputMode, mode_);

  while (true) {
    int64_t read_result = internal_file_->Read(&io_buffer_[0],
                                               io_buffer_.size());
    if (read_result <= 0) {
      NoBarrier_Store(&eof_, read_result == 0);
      NoBarrier_Store(&internal_file_error_, read_result);
      cache_.Close();
      return;
    }
    if (cache_.Write(&io_buffer_[0], read_result) == 0) {
      return;
    }
  }
}

void ThreadedIoFile::RunInOutputMode() {
  DCHECK(internal_file_);
  DCHECK(thread_);
  DCHECK_EQ(kOutputMode, mode_);

  while (true) {
    uint64_t write_bytes = cache_.Read(&io_buffer_[0], io_buffer_.size());
    if (write_bytes == 0) {
      if (flushing_) {
        cache_.Reopen();
        flushing_ = false;
        flush_complete_event_.Signal();
      } else {
        return;
      }
    } else {
      uint64_t bytes_written(0);
      while (bytes_written < write_bytes) {
        int64_t write_result = internal_file_->Write(
            &io_buffer_[bytes_written], write_bytes - bytes_written);
        if (write_result < 0) {
          NoBarrier_Store(&internal_file_error_, write_result);
          cache_.Close();
          return;
        }
        bytes_written += write_result;
      }
    }
  }
}

}  // namespace media
}  // namespace edash_packager
