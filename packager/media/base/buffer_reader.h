// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_BUFFER_READER_H_
#define PACKAGER_MEDIA_BASE_BUFFER_READER_H_

#include <cstdint>
#include <string>
#include <vector>

#include <packager/macros/classes.h>

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
  [[nodiscard]] bool Read1(uint8_t* v);
  [[nodiscard]] bool Read2(uint16_t* v);
  [[nodiscard]] bool Read2s(int16_t* v);
  [[nodiscard]] bool Read4(uint32_t* v);
  [[nodiscard]] bool Read4s(int32_t* v);
  [[nodiscard]] bool Read8(uint64_t* v);
  [[nodiscard]] bool Read8s(int64_t* v);
  /// @}

  /// Read N-byte integer of the corresponding signedness and store it in the
  /// 8-byte return type.
  /// @param num_bytes should not be larger than 8 bytes.
  /// @return false if there are not enough bytes in the buffer, true otherwise.
  /// @{
  [[nodiscard]] bool ReadNBytesInto8(uint64_t* v, size_t num_bytes);
  [[nodiscard]] bool ReadNBytesInto8s(int64_t* v, size_t num_bytes);
  /// @}

  [[nodiscard]] bool ReadToVector(std::vector<uint8_t>* t, size_t count);
  [[nodiscard]] bool ReadToString(std::string* str, size_t size);

  /// Reads a null-terminated string.
  [[nodiscard]] bool ReadCString(std::string* str);

  /// Advance the stream by this many bytes.
  /// @return false if there are not enough bytes in the buffer, true otherwise.
  [[nodiscard]] bool SkipBytes(size_t num_bytes);

  const uint8_t* data() const { return buf_; }
  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }
  size_t pos() const { return pos_; }

 private:
  // Internal implementation of multi-byte reads.
  template <typename T>
  [[nodiscard]] bool Read(T* t);
  template <typename T>
  [[nodiscard]] bool ReadNBytes(T* t, size_t num_bytes);

  const uint8_t* buf_;
  size_t size_;
  size_t pos_;

  DISALLOW_COPY_AND_ASSIGN(BufferReader);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_BUFFER_READER_H_
