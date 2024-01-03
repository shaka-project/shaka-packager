// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of an H264 Annex-B video stream parser.

#ifndef PACKAGER_MEDIA_CODECS_H264_BIT_READER_H_
#define PACKAGER_MEDIA_CODECS_H264_BIT_READER_H_

#include <sys/types.h>

#include <cstdint>

#include <packager/macros/classes.h>

namespace shaka {
namespace media {

// A class to provide bit-granularity reading of H.264/H.265 streams.
// This is not a generic bit reader class, as it takes into account
// H.264 stream-specific constraints, such as skipping emulation-prevention
// bytes and stop bits. See spec for more details.
class H26xBitReader {
 public:
  H26xBitReader();
  ~H26xBitReader();

  // Initialize the reader to start reading at |data|, |size| being size
  // of |data| in bytes.
  // Return false on insufficient size of stream..
  bool Initialize(const uint8_t* data, off_t size);

  // Read |num_bits| next bits from stream and return in |*out|, first bit
  // from the stream starting at |num_bits| position in |*out|.
  // |num_bits| may be 1-32, inclusive.
  // Return false if the given number of bits cannot be read (not enough
  // bits in the stream), true otherwise.
  bool ReadBits(int num_bits, int* out);

  // Read a single bit and return in |*out|.
  // Return false if the bit cannot be read (not enough bits in the stream),
  // true otherwise.
  bool ReadBool(bool* out) {
    int value;
    if (!ReadBits(1, &value))
      return false;
    *out = (value != 0);
    return true;
  }

  // Skips the given number of bits (does not have to be less than 32 bits).
  // Return false if there aren't enough bits in the stream, true otherwise.
  bool SkipBits(int num_bits);

  // Exp-Golomb code parsing as specified in chapter 9.1 of the spec.
  // Read one unsigned exp-Golomb code from the stream and return in |*val|.
  bool ReadUE(int* val);

  // Read one signed exp-Golomb code from the stream and return in |*val|.
  bool ReadSE(int* val);

  // Return the number of bits left in the stream.
  off_t NumBitsLeft();

  // See the definition of more_rbsp_data() in spec.
  bool HasMoreRBSPData();

  // Return the number of emulation prevention bytes already read.
  size_t NumEmulationPreventionBytesRead();

 private:
  // Advance to the next byte, loading it into curr_byte_.
  // Return false on end of stream.
  bool UpdateCurrByte();

  // Pointer to the next unread (not in curr_byte_) byte in the stream.
  const uint8_t* data_;

  // Bytes left in the stream (without the curr_byte_).
  off_t bytes_left_;

  // Contents of the current byte; first unread bit starting at position
  // 8 - num_remaining_bits_in_curr_byte_ from MSB.
  int curr_byte_;

  // Number of bits remaining in curr_byte_
  int num_remaining_bits_in_curr_byte_;

  // Used in emulation prevention three byte detection (see spec).
  // Initially set to 0xffff to accept all initial two-byte sequences.
  int prev_two_bytes_;

  // Number of emulation preventation bytes (0x000003) we met.
  size_t emulation_prevention_bytes_;

  DISALLOW_COPY_AND_ASSIGN(H26xBitReader);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_H264_BIT_READER_H_
