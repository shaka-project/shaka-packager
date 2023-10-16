// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/codecs/h26x_bit_reader.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {
namespace {

// Check if any bits in the least significant |valid_bits| are set to 1.
bool CheckAnyBitsSet(int byte, int valid_bits) {
  return (byte & ((1 << valid_bits) - 1)) != 0;
}

}  // namespace

H26xBitReader::H26xBitReader()
    : data_(NULL),
      bytes_left_(0),
      curr_byte_(0),
      num_remaining_bits_in_curr_byte_(0),
      prev_two_bytes_(0),
      emulation_prevention_bytes_(0) {}

H26xBitReader::~H26xBitReader() {}

bool H26xBitReader::Initialize(const uint8_t* data, off_t size) {
  DCHECK(data);

  if (size < 1)
    return false;

  data_ = data;
  bytes_left_ = size;
  num_remaining_bits_in_curr_byte_ = 0;
  // Initially set to 0xffff to accept all initial two-byte sequences.
  prev_two_bytes_ = 0xffff;
  emulation_prevention_bytes_ = 0;

  return true;
}

bool H26xBitReader::UpdateCurrByte() {
  if (bytes_left_ < 1)
    return false;

  // Emulation prevention three-byte detection.
  // If a sequence of 0x000003 is found, skip (ignore) the last byte (0x03).
  if (*data_ == 0x03 && (prev_two_bytes_ & 0xffff) == 0) {
    // Detected 0x000003, skip last byte.
    ++data_;
    --bytes_left_;
    ++emulation_prevention_bytes_;
    // Need another full three bytes before we can detect the sequence again.
    prev_two_bytes_ = 0xffff;

    if (bytes_left_ < 1)
      return false;
  }

  // Load a new byte and advance pointers.
  curr_byte_ = *data_++ & 0xff;
  --bytes_left_;
  num_remaining_bits_in_curr_byte_ = 8;

  prev_two_bytes_ = (prev_two_bytes_ << 8) | curr_byte_;

  return true;
}

// Read |num_bits| (1 to 31 inclusive) from the stream and return them
// in |out|, with first bit in the stream as MSB in |out| at position
// (|num_bits| - 1).
bool H26xBitReader::ReadBits(int num_bits, int* out) {
  int bits_left = num_bits;
  *out = 0;
  DCHECK(num_bits <= 31);

  while (num_remaining_bits_in_curr_byte_ < bits_left) {
    // Take all that's left in current byte, shift to make space for the rest.
    *out |= (curr_byte_ << (bits_left - num_remaining_bits_in_curr_byte_));
    bits_left -= num_remaining_bits_in_curr_byte_;

    if (!UpdateCurrByte())
      return false;
  }

  *out |= (curr_byte_ >> (num_remaining_bits_in_curr_byte_ - bits_left));
  *out &= ((1 << num_bits) - 1);
  num_remaining_bits_in_curr_byte_ -= bits_left;

  return true;
}

bool H26xBitReader::SkipBits(int num_bits) {
  int bits_left = num_bits;
  while (num_remaining_bits_in_curr_byte_ < bits_left) {
    bits_left -= num_remaining_bits_in_curr_byte_;
    if (!UpdateCurrByte())
      return false;
  }

  num_remaining_bits_in_curr_byte_ -= bits_left;
  return true;
}

bool H26xBitReader::ReadUE(int* val) {
  int num_bits = -1;
  int bit;
  int rest;

  // Count the number of contiguous zero bits.
  do {
    if (!ReadBits(1, &bit))
      return false;
    num_bits++;
  } while (bit == 0);

  if (num_bits > 31)
    return false;

  // Calculate exp-Golomb code value of size num_bits.
  *val = (1 << num_bits) - 1;

  if (num_bits > 0) {
    if (!ReadBits(num_bits, &rest))
      return false;
    *val += rest;
  }

  return true;
}

bool H26xBitReader::ReadSE(int* val) {
  int ue;

  // See Chapter 9 in the spec.
  if (!ReadUE(&ue))
    return false;

  if (ue % 2 == 0)
    *val = -(ue / 2);
  else
    *val = ue / 2 + 1;

  return true;
}

off_t H26xBitReader::NumBitsLeft() {
  return (num_remaining_bits_in_curr_byte_ + bytes_left_ * 8);
}

bool H26xBitReader::HasMoreRBSPData() {
  // Make sure we have more bits, if we are at 0 bits in current byte and
  // updating current byte fails, we don't have more data anyway.
  if (num_remaining_bits_in_curr_byte_ == 0 && !UpdateCurrByte())
    return false;

  // If there is no more RBSP data, then the remaining bits is the stop bit
  // followed by zero paddings. So if there are 1s in the remaining bits
  // excluding the current bit, then the current bit is not a stop bit,
  // regardless of whether it is 1 or not. Therefore there is more data.
  if (CheckAnyBitsSet(curr_byte_, num_remaining_bits_in_curr_byte_ - 1))
    return true;

  // While the spec disallows it (7.4.1: "The last byte of the NAL unit shall
  // not be equal to 0x00"), some streams have trailing null bytes anyway. We
  // don't handle emulation prevention sequences because HasMoreRBSPData() is
  // not used when parsing slices (where cabac_zero_word elements are legal).
  for (off_t i = 0; i < bytes_left_; i++) {
    if (data_[i] != 0)
      return true;
  }

  bytes_left_ = 0;
  return false;
}

size_t H26xBitReader::NumEmulationPreventionBytesRead() {
  return emulation_prevention_bytes_;
}

}  // namespace media
}  // namespace shaka
