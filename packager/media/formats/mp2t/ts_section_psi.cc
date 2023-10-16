// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/mp2t/ts_section_psi.h>

#include <algorithm>
#include <cstdint>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/formats/mp2t/mp2t_common.h>

static bool IsCrcValid(const uint8_t* buf, int size) {
  uint32_t crc = 0xffffffffu;
  const uint32_t kCrcPoly = 0x4c11db7;

  for (int k = 0; k < size; k++) {
    int nbits = 8;
    uint32_t data_msb_aligned = buf[k];
    data_msb_aligned <<= (32 - nbits);

    while (nbits > 0) {
      if ((data_msb_aligned ^ crc) & 0x80000000) {
        crc <<= 1;
        crc ^= kCrcPoly;
      } else {
        crc <<= 1;
      }

      data_msb_aligned <<= 1;
      nbits--;
    }
  }

  return (crc == 0);
}

namespace shaka {
namespace media {
namespace mp2t {

TsSectionPsi::TsSectionPsi()
    : wait_for_pusi_(true),
      leading_bytes_to_discard_(0) {
}

TsSectionPsi::~TsSectionPsi() {
}

bool TsSectionPsi::Parse(bool payload_unit_start_indicator,
                         const uint8_t* buf,
                         int size) {
  // Ignore partial PSI.
  if (wait_for_pusi_ && !payload_unit_start_indicator)
    return true;

  if (payload_unit_start_indicator) {
    // Reset the state of the PSI section.
    ResetPsiState();

    // Update the state.
    wait_for_pusi_ = false;
    DCHECK_GE(size, 1);
    int pointer_field = buf[0];
    leading_bytes_to_discard_ = pointer_field;
    buf++;
    size--;
  }

  // Discard some leading bytes if needed.
  if (leading_bytes_to_discard_ > 0) {
    int nbytes_to_discard = std::min(leading_bytes_to_discard_, size);
    buf += nbytes_to_discard;
    size -= nbytes_to_discard;
    leading_bytes_to_discard_ -= nbytes_to_discard;
  }
  if (size == 0)
    return true;

  // Add the data to the parser state.
  psi_byte_queue_.Push(buf, size);
  int raw_psi_size;
  const uint8_t* raw_psi;
  psi_byte_queue_.Peek(&raw_psi, &raw_psi_size);

  // Check whether we have enough data to start parsing.
  if (raw_psi_size < 3)
    return true;
  int section_length =
      ((static_cast<int>(raw_psi[1]) << 8) |
       (static_cast<int>(raw_psi[2]))) & 0xfff;
  if (section_length >= 1021)
    return false;
  int psi_length = section_length + 3;
  if (raw_psi_size < psi_length) {
    // Don't throw an error when there is not enough data,
    // just wait for more data to come.
    return true;
  }

  // There should not be any trailing bytes after a PMT.
  // Instead, the pointer field should be used to stuff bytes.
  if (raw_psi_size > psi_length) {
    DVLOG(1) << "Trailing bytes after a PSI section: " << psi_length << " vs "
             << raw_psi_size;
  }

  // Verify the CRC.
  RCHECK(IsCrcValid(raw_psi, psi_length));

  // Parse the PSI section.
  BitReader bit_reader(raw_psi, raw_psi_size);
  bool status = ParsePsiSection(&bit_reader);
  if (status)
    ResetPsiState();

  return status;
}

bool TsSectionPsi::Flush() {
  return true;
}

void TsSectionPsi::Reset() {
  ResetPsiSection();
  ResetPsiState();
}

void TsSectionPsi::ResetPsiState() {
  wait_for_pusi_ = true;
  psi_byte_queue_.Reset();
  leading_bytes_to_discard_ = 0;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
