// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_BUFFER_READER_H_
#define PACKAGER_MEDIA_BASE_BUFFER_READER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "packager/base/compiler_specific.h"
#include "packager/base/macros.h"

namespace shaka {
namespace media {

/// A simple buffer reader implementation, which reads data of various types
/// from a fixed byte array.
class BufferReader {
 public:
  /// Create a BufferReader from a raw buffer.
  BufferReader(const uint8_t* buf, size_t size)
      : buf_(buf), size_(size), pos_(0) {}
  ~BufferReader() {}

  /// @return true if there are more than @a count bytes in the stream, false
  ///         otherwise.
  bool HasBytes(size_t count) { return pos() + count <= size(); }

  /// Read a value from the stream, performing endian correction, and advance
  /// the stream pointer.
  /// @return false if there are not enough bytes in the buffer.
  /// @{
  bool Read1(uint8_t* v) WARN_UNUSED_RESULT;
  bool Read2(uint16_t* v) WARN_UNUSED_RESULT;
  bool Read2s(int16_t* v) WARN_UNUSED_RESULT;
  bool Read4(uint32_t* v) WARN_UNUSED_RESULT;
  bool Read4s(int32_t* v) WARN_UNUSED_RESULT;
  bool Read8(uint64_t* v) WARN_UNUSED_RESULT;
  bool Read8s(int64_t* v) WARN_UNUSED_RESULT;
  /// @}

  /// Read N-byte integer of the corresponding signedness and store it in the
  /// 8-byte return type.
  /// @param num_bytes should not be larger than 8 bytes.
  /// @return false if there are not enough bytes in the buffer, true otherwise.
  /// @{
  bool ReadNBytesInto8(uint64_t* v, size_t num_bytes) WARN_UNUSED_RESULT;
  bool ReadNBytesInto8s(int64_t* v, size_t num_bytes) WARN_UNUSED_RESULT;
  /// @}

  bool ReadToVector(std::vector<uint8_t>* t, size_t count) WARN_UNUSED_RESULT;
  bool ReadToString(std::string* str, size_t size) WARN_UNUSED_RESULT;

  /// Advance the stream by this many bytes.
  /// @return false if there are not enough bytes in the buffer, true otherwise.
  bool SkipBytes(size_t num_bytes) WARN_UNUSED_RESULT;

  const uint8_t* data() const { return buf_; }
  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }
  size_t pos() const { return pos_; }

 private:
  // Internal implementation of multi-byte reads.
  template <typename T>
  bool Read(T* t) WARN_UNUSED_RESULT;
  template <typename T>
  bool ReadNBytes(T* t, size_t num_bytes) WARN_UNUSED_RESULT;

  const uint8_t* buf_;
  size_t size_;
  size_t pos_;

  DISALLOW_COPY_AND_ASSIGN(BufferReader);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_BUFFER_READER_H_
