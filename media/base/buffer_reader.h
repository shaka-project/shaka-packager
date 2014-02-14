// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_BUFFER_READER_H_
#define MEDIA_BASE_BUFFER_READER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace media {

// A simple buffer reader implementation, which reads data of various type
// from a fixed byte array.
class BufferReader {
 public:
  BufferReader(const uint8* buf, size_t size)
      : buf_(buf), size_(size), pos_(0) {}
  ~BufferReader() {}

  bool HasBytes(size_t count) { return pos() + count <= size(); }

  // Read a value from the stream, performing endian correction, and advance
  // the stream pointer.
  // Return false if there are not enough bytes in the buffer.
  bool Read1(uint8* v) WARN_UNUSED_RESULT;
  bool Read2(uint16* v) WARN_UNUSED_RESULT;
  bool Read2s(int16* v) WARN_UNUSED_RESULT;
  bool Read4(uint32* v) WARN_UNUSED_RESULT;
  bool Read4s(int32* v) WARN_UNUSED_RESULT;
  bool Read8(uint64* v) WARN_UNUSED_RESULT;
  bool Read8s(int64* v) WARN_UNUSED_RESULT;
  // Read a N-byte integer of the corresponding signedness and store it in the
  // 8-byte return type. |num_bytes| should not be larger than 8 bytes.
  // Return false if there are not enough bytes in the buffer.
  bool ReadNBytesInto8(uint64* v, size_t num_bytes) WARN_UNUSED_RESULT;
  bool ReadNBytesInto8s(int64* v, size_t num_bytes) WARN_UNUSED_RESULT;

  bool ReadToVector(std::vector<uint8>* t, size_t count) WARN_UNUSED_RESULT;

  // Advance the stream by this many bytes.
  // Return false if there are not enough bytes in the buffer.
  bool SkipBytes(size_t num_bytes) WARN_UNUSED_RESULT;

  const uint8* data() const { return buf_; }
  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }
  size_t pos() const { return pos_; }

 private:
  // Internal implementation of multi-byte reads.
  template <typename T>
  bool Read(T* t) WARN_UNUSED_RESULT;
  template <typename T>
  bool ReadNBytes(T* t, size_t num_bytes) WARN_UNUSED_RESULT;

  const uint8* buf_;
  size_t size_;
  size_t pos_;

  DISALLOW_COPY_AND_ASSIGN(BufferReader);
};

}  // namespace media

#endif  // MEDIA_BASE_BUFFER_READER_H_
