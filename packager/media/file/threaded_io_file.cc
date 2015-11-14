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

  return cache_.Read(buffer, length);
}

int64_t ThreadedIoFile::Write(const void* buffer, uint64_t length) {
  DCHECK(internal_file_);
  DCHECK(thread_);
  DCHECK_EQ(kOutputMode, mode_);

  if (NoBarrier_Load(&internal_file_error_))
    return NoBarrier_Load(&internal_file_error_);

  size_ += length;
  return cache_.Write(buffer, length);
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
    cache_.Write(&io_buffer_[0], read_result);
  }
}

bool ThreadedIoFile::Seek(uint64_t position) {
  NOTIMPLEMENTED();
  return false;
}

bool ThreadedIoFile::Tell(uint64_t* position) {
  NOTIMPLEMENTED();
  return false;
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
