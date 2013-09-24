// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BIT_READER_H_
#define MEDIA_BASE_BIT_READER_H_

#include <sys/types.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/base/media_export.h"

namespace media {

// A class to read bit streams.
class MEDIA_EXPORT BitReader {
 public:
  // Initialize the reader to start reading at |data|, |size| being size
  // of |data| in bytes.
  BitReader(const uint8* data, off_t size);
  ~BitReader();

  // Read |num_bits| next bits from stream and return in |*out|, first bit
  // from the stream starting at |num_bits| position in |*out|.
  // |num_bits| cannot be larger than the bits the type can hold.
  // Return false if the given number of bits cannot be read (not enough
  // bits in the stream), true otherwise. When return false, the stream will
  // enter a state where further ReadBits/SkipBits operations will always
  // return false unless |num_bits| is 0. The type |T| has to be a primitive
  // integer type.
  template<typename T> bool ReadBits(int num_bits, T *out) {
    DCHECK_LE(num_bits, static_cast<int>(sizeof(T) * 8));
    uint64 temp;
    bool ret = ReadBitsInternal(num_bits, &temp);
    *out = static_cast<T>(temp);
    return ret;
  }

  // Skip |num_bits| next bits from stream. Return false if the given number of
  // bits cannot be skipped (not enough bits in the stream), true otherwise.
  // When return false, the stream will enter a state where further ReadBits/
  // SkipBits operations will always return false unless |num_bits| is 0.
  bool SkipBits(int num_bits);

  // Returns the number of bits available for reading.
  int bits_available() const;

 private:
  // Help function used by ReadBits to avoid inlining the bit reading logic.
  bool ReadBitsInternal(int num_bits, uint64* out);

  // Advance to the next byte, loading it into curr_byte_.
  // If the num_remaining_bits_in_curr_byte_ is 0 after this function returns,
  // the stream has reached the end.
  void UpdateCurrByte();

  // Pointer to the next unread (not in curr_byte_) byte in the stream.
  const uint8* data_;

  // Bytes left in the stream (without the curr_byte_).
  off_t bytes_left_;

  // Contents of the current byte; first unread bit starting at position
  // 8 - num_remaining_bits_in_curr_byte_ from MSB.
  uint8 curr_byte_;

  // Number of bits remaining in curr_byte_
  int num_remaining_bits_in_curr_byte_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitReader);
};

}  // namespace media

#endif  // MEDIA_BASE_BIT_READER_H_
