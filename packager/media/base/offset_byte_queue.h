// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_BASE_OFFSET_BYTE_QUEUE_H_
#define PACKAGER_MEDIA_BASE_OFFSET_BYTE_QUEUE_H_

#include <cstdint>

#include <packager/macros/classes.h>
#include <packager/media/base/byte_queue.h>

namespace shaka {
namespace media {

/// Wrapper around ByteQueue, which encapsulates the notion of a
/// monotonically-increasing byte offset. All buffer access is done by passing
/// these offsets into this class, reducing the proliferation of many different
/// meanings of "offset", "head", etc.
class OffsetByteQueue {
 public:
  OffsetByteQueue();
  ~OffsetByteQueue();

  /// @name These work like their underlying ByteQueue counterparts.
  /// @{
  void Reset();
  void Push(const uint8_t* buf, int size);
  void Peek(const uint8_t** buf, int* size);
  void Pop(int count);
  /// @}

  /// Set @a buf to point at the first buffered byte corresponding to @a offset,
  /// and @a size to the number of bytes available starting from that offset.
  ///
  /// It is an error if the offset is before the current head. It's not an error
  /// if the current offset is beyond tail(), but you will of course get back
  /// a null @a buf and a @a size of zero.
  void PeekAt(int64_t offset, const uint8_t** buf, int* size);

  /// Mark the bytes up to (but not including) @a max_offset as ready for
  /// deletion. This is relatively inexpensive, but will not necessarily reduce
  /// the resident buffer size right away (or ever).
  ///
  /// @return true if the full range of bytes were successfully trimmed,
  ///         including the case where @a max_offset is less than the current
  ///         head.
  /// @return false if @a max_offset > tail() (although all bytes currently
  ///         buffered are still cleared).
  bool Trim(int64_t max_offset);

  /// @return The head position, in terms of the file's absolute offset.
  int64_t head() { return head_; }
  /// @return The tail position (exclusive), in terms of the file's absolute
  ///         offset.
  int64_t tail() { return head_ + size_; }

 private:
  // Synchronize |buf_| and |size_| with |queue_|.
  void Sync();

  ByteQueue queue_;
  const uint8_t* buf_;
  int size_;
  int64_t head_;

  DISALLOW_COPY_AND_ASSIGN(OffsetByteQueue);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_OFFSET_BYTE_QUEUE_H_
