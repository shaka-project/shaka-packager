// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_BASE_BIT_READER_H_
#define PACKAGER_MEDIA_BASE_BIT_READER_H_

#include <cstddef>
#include <cstdint>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/classes.h>

namespace shaka {
namespace media {

/// A class to read bit streams.
class BitReader {
 public:
  /// Initialize the BitReader object to read a data buffer.
  /// @param data points to the beginning of the buffer.
  /// @param size is the buffer size in bytes.
  BitReader(const uint8_t* data, size_t size);
  ~BitReader();

  /// Read a number of bits from stream.
  /// @param num_bits specifies the number of bits to read. It cannot be larger
  ///        than the number of bits the type can hold.
  /// @param[out] out stores the output. The type @b T has to be a primitive
  ///             integer type.
  /// @return false if the given number of bits cannot be read (not enough
  ///         bits in the stream), true otherwise. When false is returned, the
  ///         stream will enter a state where further ReadBits/SkipBits
  ///         operations will always return false unless @a num_bits is 0.
  template <typename T>
  bool ReadBits(size_t num_bits, T* out) {
    DCHECK_LE(num_bits, sizeof(T) * 8);
    uint64_t temp;
    bool ret = ReadBitsInternal(num_bits, &temp);
    *out = static_cast<T>(temp);
    return ret;
  }

  // Explicit T=bool overload to make MSVC happy.
  bool ReadBits(size_t num_bits, bool* out) {
    DCHECK_EQ(num_bits, 1u);
    uint64_t temp;
    bool ret = ReadBitsInternal(num_bits, &temp);
    *out = temp != 0;
    return ret;
  }

  /// Skip a number of bits from stream.
  /// @param num_bits specifies the number of bits to be skipped.
  /// @return false if the given number of bits cannot be skipped (not enough
  ///         bits in the stream), true otherwise. When false is returned, the
  ///         stream will enter a state where further ReadXXX/SkipXXX
  ///         operations will always return false unless |num_bits/bytes| is 0.
  bool SkipBits(size_t num_bits);

  /// Read one bit then skip the number of bits specified if that bit matches @a
  /// condition.
  /// @param condition indicates when the number of bits should be skipped.
  /// @param num_bits specifies the number of bits to be skipped.
  /// @return false if the one bit cannot be read (not enough bits in the
  ///         stream) or if the bit is set but the given number of bits cannot
  ///         be skipped (not enough bits in the stream), true otherwise. When
  ///         false is returned, the stream will enter a state where further
  ///         ReadXXX/SkipXXX operations will always return false.
  bool SkipBitsConditional(bool condition, size_t num_bits) {
    bool condition_read = true;
    if (!ReadBits(1, &condition_read))
      return false;
    return condition_read == condition ? SkipBits(num_bits) : true;
  }

  /// Skip a number of bits so the stream is byte aligned to the initial data.
  /// There could be 0 to 7 bits skipped.
  void SkipToNextByte();

  /// Skip a number of bytes from stream. The current posision should be byte
  /// aligned, otherwise a false is returned and bytes are not skipped.
  /// @param num_bytes specifies the number of bytes to be skipped.
  /// @return false if the current position is not byte aligned or if the given
  ///         number of bytes cannot be skipped (not enough bytes in the
  ///         stream), true otherwise.
  bool SkipBytes(size_t num_bytes);

  /// @return The number of bits available for reading.
  size_t bits_available() const {
    return 8 * bytes_left_ + num_remaining_bits_in_curr_byte_;
  }

  /// @return The current bit position.
  size_t bit_position() const { return 8 * initial_size_ - bits_available(); }

  /// @return A pointer to the current byte.
  const uint8_t* current_byte_ptr() const { return data_ - 1; }

 private:
  // Help function used by ReadBits to avoid inlining the bit reading logic.
  bool ReadBitsInternal(size_t num_bits, uint64_t* out);

  // Advance to the next byte, loading it into curr_byte_.
  // If the num_remaining_bits_in_curr_byte_ is 0 after this function returns,
  // the stream has reached the end.
  void UpdateCurrByte();

  // Pointer to the next unread (not in curr_byte_) byte in the stream.
  const uint8_t* data_;

  // Initial size of the input data.
  size_t initial_size_;

  // Bytes left in the stream (without the curr_byte_).
  size_t bytes_left_;

  // Contents of the current byte; first unread bit starting at position
  // 8 - num_remaining_bits_in_curr_byte_ from MSB.
  uint8_t curr_byte_;

  // Number of bits remaining in curr_byte_
  size_t num_remaining_bits_in_curr_byte_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitReader);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_BIT_READER_H_
