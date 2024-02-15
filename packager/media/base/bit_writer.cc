// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/bit_writer.h>

#include <absl/log/check.h>

namespace shaka {
namespace media {

BitWriter::BitWriter(std::vector<uint8_t>* storage)
    : storage_(storage), initial_storage_size_(storage_->size()) {}

void BitWriter::WriteBits(uint32_t bits, size_t number_of_bits) {
  DCHECK_NE(number_of_bits, 0u);
  DCHECK_LE(number_of_bits, 32u);
  DCHECK_LT(bits, 1ULL << number_of_bits);

  num_bits_ += number_of_bits;
  DCHECK_LE(num_bits_, 64);
  bits_ |= static_cast<uint64_t>(bits) << (64 - num_bits_);

  while (num_bits_ >= 8) {
    storage_->push_back(bits_ >> 56);
    bits_ <<= 8;
    num_bits_ -= 8;
  }
}

void BitWriter::Flush() {
  while (num_bits_ > 0) {
    storage_->push_back(bits_ >> 56);
    bits_ <<= 8;
    num_bits_ -= 8;
  }
  bits_ = 0;
  num_bits_ = 0;
}

}  // namespace media
}  // namespace shaka
