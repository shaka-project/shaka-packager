// Copyright (c) 2023 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/codecs/dts_audio_specific_config.h>

#include <packager/media/base/bit_reader.h>
#include <packager/media/base/rcheck.h>

namespace shaka {
namespace media {

bool GetDTSXChannelMask(const std::vector<uint8_t>& udts, uint32_t& mask) {
  // udts is the DTS-UHD Specific Box: ETSI TS 103 491 V1.2.1 Table B-2
  // DecoderProfileCode(6 bits)
  // FrameDurationCode(2 bits)
  // MaxPayloadCode(3 bits)
  // NumPresentationsCode(5 bits)
  // ChannelMask (32 bits)
  BitReader bit_reader(udts.data(), udts.size());
  RCHECK(bit_reader.SkipBits(16));
  RCHECK(bit_reader.ReadBits(32, &mask));
  return true;
}

}  // namespace media
}  // namespace shaka
