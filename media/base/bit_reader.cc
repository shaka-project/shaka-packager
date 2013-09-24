// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bit_reader.h"

namespace media {

BitReader::BitReader(const uint8* data, off_t size)
    : data_(data), bytes_left_(size), num_remaining_bits_in_curr_byte_(0) {
  DCHECK(data_ != NULL && bytes_left_ > 0);

  UpdateCurrByte();
}

BitReader::~BitReader() {}

bool BitReader::SkipBits(int num_bits) {
  DCHECK_GE(num_bits, 0);
  DLOG_IF(INFO, num_bits > 100)
      << "BitReader::SkipBits inefficient for large skips";

  // Skip any bits in the current byte waiting to be processed, then
  // process full bytes until less than 8 bits remaining.
  while (num_bits > 0 && num_bits > num_remaining_bits_in_curr_byte_) {
    num_bits -= num_remaining_bits_in_curr_byte_;
    num_remaining_bits_in_curr_byte_ = 0;
    UpdateCurrByte();

    // If there is no more data remaining, only return true if we
    // skipped all that were requested.
    if (num_remaining_bits_in_curr_byte_ == 0)
      return (num_bits == 0);
  }

  // Less than 8 bits remaining to skip. Use ReadBitsInternal to verify
  // that the remaining bits we need exist, and adjust them as necessary
  // for subsequent operations.
  uint64 not_needed;
  return ReadBitsInternal(num_bits, &not_needed);
}

int BitReader::bits_available() const {
  return 8 * bytes_left_ + num_remaining_bits_in_curr_byte_;
}

bool BitReader::ReadBitsInternal(int num_bits, uint64* out) {
  DCHECK_LE(num_bits, 64);

  *out = 0;

  while (num_remaining_bits_in_curr_byte_ != 0 && num_bits != 0) {
    int bits_to_take = std::min(num_remaining_bits_in_curr_byte_, num_bits);

    *out <<= bits_to_take;
    *out += curr_byte_ >> (num_remaining_bits_in_curr_byte_ - bits_to_take);
    num_bits -= bits_to_take;
    num_remaining_bits_in_curr_byte_ -= bits_to_take;
    curr_byte_ &= (1 << num_remaining_bits_in_curr_byte_) - 1;

    if (num_remaining_bits_in_curr_byte_ == 0)
      UpdateCurrByte();
  }

  return num_bits == 0;
}

void BitReader::UpdateCurrByte() {
  DCHECK_EQ(num_remaining_bits_in_curr_byte_, 0);

  if (bytes_left_ == 0)
    return;

  // Load a new byte and advance pointers.
  curr_byte_ = *data_;
  ++data_;
  --bytes_left_;
  num_remaining_bits_in_curr_byte_ = 8;
}

}  // namespace media
