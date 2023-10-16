// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_BIT_WRITER_H_
#define PACKAGER_MEDIA_BASE_BIT_WRITER_H_

#include <cstdint>
#include <vector>

#include <absl/log/log.h>

namespace shaka {
namespace media {

class BitWriter {
 public:
  /// Constructor a BitWriter instance which writes to the provided storage.
  /// @param storage points to vector this BitWriter writes to. Cannot be
  ///        nullptr.
  explicit BitWriter(std::vector<uint8_t>* storage);
  ~BitWriter() = default;

  /// Appends the sequence 'bits' of length 'number_of_bits' <= 32.
  /// Note that 'bits' should contain at most 'number_of_bits' bits, i.e.
  /// bits should be less than 1 << number_of_bits.
  /// @param bits is the data to write.
  /// @param number_of_bits is the number of LSB to write, capped at 32. Cannot
  ///        be zero.
  void WriteBits(uint32_t bits, size_t number_of_bits);

  /// Write pending bits, and align bitstream with extra zero bits.
  void Flush();

  /// @return last written position, in bits.
  size_t BitPos() const { return BytePos() * 8 + num_bits_; }

  /// @return last written position, in bytes.
  size_t BytePos() const { return storage_->size() - initial_storage_size_; }

 private:
  BitWriter(const BitWriter&) = delete;
  BitWriter& operator=(const BitWriter&) = delete;

  // Accumulator for unwritten bits.
  uint64_t bits_ = 0;
  // Number of unwritten bits.
  int num_bits_ = 0;
  // Buffer contains the written bits.
  std::vector<uint8_t>* const storage_ = nullptr;
  const size_t initial_storage_size_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_BIT_WRITER_H_
