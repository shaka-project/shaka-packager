// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/threaded_io_file.h"

#include "packager/base/bind.h"
#include "packager/base/bind_helpers.h"
#include "packager/base/location.h"
#include "packager/base/threading/worker_pool.h"

namespace shaka {

ThreadedIoFile::ThreadedIoFile(std::unique_ptr<File, FileCloser> internal_file,
                               Mode mode,
                               uint64_t io_cache_size,
                               uint64_t io_block_size)
    : File(internal_file->file_name()),
      internal_file_(std::move(internal_file)),
      mode_(mode),
      cache_(io_cache_size),
      io_buffer_(io_block_size),
      position_(0),
      size_(0),
      eof_(false),
      flushing_(false),
      flush_complete_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED),
      internal_file_error_(0),
      task_exit_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(internal_file_);
}

ThreadedIoFile::~ThreadedIoFile() {}

bool ThreadedIoFile::Open() {
  DCHECK(internal_file_);

  if (!internal_file_->Open())
    return false;

  position_ = 0;
  size_ = internal_file_->Size();

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&ThreadedIoFile::TaskHandler, base::Unretained(this)),
      true /* task_is_slow */);
  return true;
}

bool ThreadedIoFile::Close() {
  DCHECK(internal_file_);

  bool result = true;
  if (mode_ == kOutputMode)
    result = Flush();

  cache_.Close();
  task_exit_event_.Wait();

  result &= internal_file_.release()->Close();
  delete this;
  return result;
}

int64_t ThreadedIoFile::Read(void* buffer, uint64_t length) {
  DCHECK(internal_file_);
  DCHECK_EQ(kInputMode, mode_);

  if (eof_.load(std::memory_order_relaxed) && !cache_.BytesCached())
    return 0;

  if (internal_file_error_.load(std::memory_order_relaxed))
    return internal_file_error_.load(std::memory_order_relaxed);

  uint64_t bytes_read = cache_.Read(buffer, length);
  position_ += bytes_read;

  return bytes_read;
}

int64_t ThreadedIoFile::Write(const void* buffer, uint64_t length) {
  DCHECK(internal_file_);
  DCHECK_EQ(kOutputMode, mode_);

  if (internal_file_error_.load(std::memory_order_relaxed))
    return internal_file_error_.load(std::memory_order_relaxed);

  uint64_t bytes_written = cache_.Write(buffer, length);
  position_ += bytes_written;
  if (position_ > size_)
    size_ = position_;

  return bytes_written;
}

int64_t ThreadedIoFile::Size() {
  DCHECK(internal_file_);

  return size_;
}

bool ThreadedIoFile::Flush() {
  DCHECK(internal_file_);
  DCHECK_EQ(kOutputMode, mode_);

  if (internal_file_error_.load(std::memory_order_relaxed))
    return false;

  flushing_ = true;
  cache_.Close();
  flush_complete_event_.Wait();
  return internal_file_->Flush();
}

bool ThreadedIoFile::Seek(uint64_t position) {
  if (mode_ == kOutputMode) {
    // Writing. Just flush the cache and seek.
    if (!Flush())
      return false;
    if (!internal_file_->Seek(position))
      return false;
  } else {
    // Reading. Close cache, wait for thread task to exit, seek, and re-post
    // the task.
    cache_.Close();
    task_exit_event_.Wait();
    bool result = internal_file_->Seek(position);
    if (!result) {
      // Seek failed. Seek to logical position instead.
      if (!internal_file_->Seek(position_) && (position != position_)) {
        LOG(WARNING) << "Seek failed. ThreadedIoFile left in invalid state.";
      }
    }
    cache_.Reopen();
    eof_ = false;
    base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(&ThreadedIoFile::TaskHandler, base::Unretained(this)),
        true /* task_is_slow */);
    if (!result)
      return false;
  }
  position_ = position;
  return true;
}

bool ThreadedIoFile::Tell(uint64_t* position) {
  DCHECK(position);

  *position = position_;
  return true;
}

void ThreadedIoFile::TaskHandler() {
  if (mode_ == kInputMode)
    RunInInputMode();
  else
    RunInOutputMode();
  task_exit_event_.Signal();
}

void ThreadedIoFile::RunInInputMode() {
  DCHECK(internal_file_);
  DCHECK_EQ(kInputMode, mode_);

  while (true) {
    int64_t read_result =
        internal_file_->Read(&io_buffer_[0], io_buffer_.size());
    if (read_result <= 0) {
      eof_.store(read_result == 0, std::memory_order_relaxed);
      internal_file_error_.store(read_result, std::memory_order_relaxed);
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
          internal_file_error_.store(write_result, std::memory_order_relaxed);
          cache_.Close();
          if (flushing_) {
            flushing_ = false;
            flush_complete_event_.Signal();
          }
          return;
        }
        bytes_written += write_result;
      }
    }
  }
}

}  // namespace shaka
