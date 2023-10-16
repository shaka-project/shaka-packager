// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_IO_CACHE_H_
#define PACKAGER_FILE_IO_CACHE_H_

#include <cstdint>
#include <vector>

#include <absl/synchronization/mutex.h>

#include <packager/macros/classes.h>

namespace shaka {

/// Declaration of class which implements a thread-safe circular buffer.
class IoCache {
 public:
  explicit IoCache(uint64_t cache_size);
  ~IoCache();

  /// Read data from the cache. This function may block until there is data in
  /// the cache.
  /// @param buffer is a buffer into which to read the data from the cache.
  /// @param size is the size of @a buffer.
  /// @return the number of bytes read into @a buffer, or 0 if the call
  ///         unblocked because the cache has been closed and is empty.
  uint64_t Read(void* buffer, uint64_t size);

  /// Write data to the cache. This function may block until there is enough
  /// room in the cache.
  /// @param buffer is a buffer containing the data to be written to the cache.
  /// @param size is the size of the data to be written to the cache.
  /// @return the amount of data written to the buffer (which will equal
  ///         @a data), or 0 if the call unblocked because the cache has been
  ///         closed.
  uint64_t Write(const void* buffer, uint64_t size);

  /// Empties the cache.
  void Clear();

  /// Close the cache. This will call any blocking calls to unblock, and the
  /// cache won't be usable until Reopened.
  void Close();

  /// @return true if the cache is closed, false otherwise.
  bool closed() { return closed_; }

  /// Reopens the cache. Any data still in the cache will be lost.
  void Reopen();

  /// Returns the number of bytes in the cache.
  /// @return the number of bytes in the cache.
  uint64_t BytesCached();

  /// Returns the number of free bytes in the cache.
  /// @return the number of free bytes in the cache.
  uint64_t BytesFree();

  /// Waits until the cache is empty or has been closed.
  void WaitUntilEmptyOrClosed();

 private:
  uint64_t BytesCachedInternal();
  uint64_t BytesFreeInternal();

  const uint64_t cache_size_;
  absl::Mutex mutex_;
  absl::CondVar read_event_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar write_event_ ABSL_GUARDED_BY(mutex_);
  std::vector<uint8_t> circular_buffer_ ABSL_GUARDED_BY(mutex_);
  const uint8_t* end_ptr_ ABSL_GUARDED_BY(mutex_);
  uint8_t* r_ptr_ ABSL_GUARDED_BY(mutex_);
  uint8_t* w_ptr_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(IoCache);
};

}  // namespace shaka

#endif  // PACKAGER_FILE_IO_CACHE_H
