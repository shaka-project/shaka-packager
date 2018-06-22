// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/io_cache.h"

#include <string.h>

#include <algorithm>

#include "packager/base/logging.h"

namespace shaka {

using base::AutoLock;
using base::AutoUnlock;

IoCache::IoCache(uint64_t cache_size)
    : cache_size_(cache_size),
      read_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                  base::WaitableEvent::InitialState::NOT_SIGNALED),
      write_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      // Make the buffer one byte larger than the cache so that when the
      // condition r_ptr == w_ptr is unambiguous (buffer empty).
      circular_buffer_(cache_size + 1),
      end_ptr_(&circular_buffer_[0] + cache_size + 1),
      r_ptr_(circular_buffer_.data()),
      w_ptr_(circular_buffer_.data()),
      closed_(false) {}

IoCache::~IoCache() {
  Close();
}

uint64_t IoCache::Read(void* buffer, uint64_t size) {
  DCHECK(buffer);

  AutoLock lock(lock_);
  while (!closed_ && (BytesCachedInternal() == 0)) {
    AutoUnlock unlock(lock_);
    write_event_.Wait();
  }

  size = std::min(size, BytesCachedInternal());
  uint64_t first_chunk_size(
      std::min(size, static_cast<uint64_t>(end_ptr_ - r_ptr_)));
  memcpy(buffer, r_ptr_, first_chunk_size);
  r_ptr_ += first_chunk_size;
  DCHECK_GE(end_ptr_, r_ptr_);
  if (r_ptr_ == end_ptr_)
    r_ptr_ = &circular_buffer_[0];
  uint64_t second_chunk_size(size - first_chunk_size);
  if (second_chunk_size) {
    memcpy(static_cast<uint8_t*>(buffer) + first_chunk_size, r_ptr_,
           second_chunk_size);
    r_ptr_ += second_chunk_size;
    DCHECK_GT(end_ptr_, r_ptr_);
  }
  read_event_.Signal();
  return size;
}

uint64_t IoCache::Write(const void* buffer, uint64_t size) {
  DCHECK(buffer);

  const uint8_t* r_ptr(static_cast<const uint8_t*>(buffer));
  uint64_t bytes_left(size);
  while (bytes_left) {
    AutoLock lock(lock_);
    while (!closed_ && (BytesFreeInternal() == 0)) {
      AutoUnlock unlock(lock_);
      VLOG(1) << "Circular buffer is full, which can happen if data arrives "
                 "faster than being consumed by packager. Ignore if it is not "
                 "live packaging. Otherwise, try increasing --io_cache_size.";
      read_event_.Wait();
    }
    if (closed_)
      return 0;

    uint64_t write_size(std::min(bytes_left, BytesFreeInternal()));
    uint64_t first_chunk_size(
        std::min(write_size, static_cast<uint64_t>(end_ptr_ - w_ptr_)));
    memcpy(w_ptr_, r_ptr, first_chunk_size);
    w_ptr_ += first_chunk_size;
    DCHECK_GE(end_ptr_, w_ptr_);
    if (w_ptr_ == end_ptr_)
      w_ptr_ = &circular_buffer_[0];
    r_ptr += first_chunk_size;
    uint64_t second_chunk_size(write_size - first_chunk_size);
    if (second_chunk_size) {
      memcpy(w_ptr_, r_ptr, second_chunk_size);
      w_ptr_ += second_chunk_size;
      DCHECK_GT(end_ptr_, w_ptr_);
      r_ptr += second_chunk_size;
    }
    bytes_left -= write_size;
    write_event_.Signal();
  }
  return size;
}

void IoCache::Clear() {
  AutoLock lock(lock_);
  r_ptr_ = w_ptr_ = circular_buffer_.data();
  // Let any writers know that there is room in the cache.
  read_event_.Signal();
}

void IoCache::Close() {
  AutoLock lock(lock_);
  closed_ = true;
  read_event_.Signal();
  write_event_.Signal();
}

void IoCache::Reopen() {
  AutoLock lock(lock_);
  CHECK(closed_);
  r_ptr_ = w_ptr_ = circular_buffer_.data();
  closed_ = false;
  read_event_.Reset();
  write_event_.Reset();
}

uint64_t IoCache::BytesCached() {
  AutoLock lock(lock_);
  return BytesCachedInternal();
}

uint64_t IoCache::BytesFree() {
  AutoLock lock(lock_);
  return BytesFreeInternal();
}

uint64_t IoCache::BytesCachedInternal() {
  return (r_ptr_ <= w_ptr_)
             ? w_ptr_ - r_ptr_
             : (end_ptr_ - r_ptr_) + (w_ptr_ - circular_buffer_.data());
}

uint64_t IoCache::BytesFreeInternal() {
  return cache_size_ - BytesCachedInternal();
}

void IoCache::WaitUntilEmptyOrClosed() {
  AutoLock lock(lock_);
  while (!closed_ && BytesCachedInternal()) {
    AutoUnlock unlock(lock_);
    read_event_.Wait();
  }
}

}  // namespace shaka
