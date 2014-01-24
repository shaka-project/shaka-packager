// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_BUFFER_WRITER_H_
#define MEDIA_BASE_BUFFER_WRITER_H_

#include <vector>

#include "base/stl_util.h"
#include "media/base/status.h"

namespace media {

class File;

/// A simple buffer writer implementation which appends various data types to
/// buffer.
class BufferWriter {
 public:
  BufferWriter();
  /// Construct the object with a reserved capacity.
  /// @param reserved_size_in_bytes is intended for optimization and is
  ///        not a hard limit. It does not affect the actual size of the buffer,
  ///        which still starts from zero.
  explicit BufferWriter(size_t reserved_size_in_bytes);
  ~BufferWriter();

  /// These convenience functions append the integers (in network byte order,
  /// i.e. big endian) of various size and signedness to the end of the buffer.
  /// @{
  void AppendInt(uint8 v);
  void AppendInt(uint16 v);
  void AppendInt(uint32 v);
  void AppendInt(uint64 v);
  void AppendInt(int16 v);
  void AppendInt(int32 v);
  void AppendInt(int64 v);
  /// @}

  /// Append the least significant @a num_bytes of @a v to buffer.
  /// @param num_bytes should not be larger than sizeof(@a v), i.e. 8 on a
  ///        64-bit system.
  void AppendNBytes(uint64 v, size_t num_bytes);

  void AppendVector(const std::vector<uint8>& v);
  void AppendArray(const uint8* buf, size_t size);
  void AppendBuffer(const BufferWriter& buffer);

  void Swap(BufferWriter* buffer) { buf_.swap(buffer->buf_); }
  void Clear() { buf_.clear(); }
  size_t Size() const { return buf_.size(); }
  /// @return Underlying buffer. Behavior is undefined if the buffer size is 0.
  const uint8* Buffer() const { return vector_as_array(&buf_); }

  /// Write the buffer to file. The internal buffer will be cleared after
  /// writing.
  /// @param file should not be NULL.
  /// @return OK on success.
  Status WriteToFile(File* file);

 private:
  // Internal implementation of multi-byte write.
  template <typename T>
  void AppendInternal(T v);

  std::vector<uint8> buf_;

  DISALLOW_COPY_AND_ASSIGN(BufferWriter);
};

}  // namespace media

#endif  // MEDIA_BASE_BUFFER_WRITER_H_
