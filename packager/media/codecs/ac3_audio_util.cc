// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/ac3_audio_util.h"

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/rcheck.h"

namespace shaka {
namespace media {

namespace {

// ASTC Standard A/52:2012 Table 5.8 Audio Coding Mode.
const uint8_t kAc3NumChannelsTable[] = {2, 1, 2, 3, 3, 4, 4, 5};

bool ExtractAc3Data(const std::vector<uint8_t>& ac3_data,
                    uint8_t* audio_coding_mode,
                    bool* lfe_channel_on) {
  BitReader bit_reader(ac3_data.data(), ac3_data.size());

  // fscod: 2 bits
  // bsid: 5 bits
  // bsmod: 3 bits
  // acmod: 3 bits
  // lfeon: 1 bit
  // bit_rate_code: 5 bits
  RCHECK(bit_reader.SkipBits(10));
  RCHECK(bit_reader.ReadBits(3, audio_coding_mode));
  RCHECK(bit_reader.ReadBits(1, lfe_channel_on));
  return true;
}

}  // namespace

size_t GetAc3NumChannels(const std::vector<uint8_t>& ac3_data) {
  uint8_t audio_coding_mode;
  bool lfe_channel_on;
  if (!ExtractAc3Data(ac3_data, &audio_coding_mode, &lfe_channel_on)) {
    LOG(WARNING) << "Seeing invalid AC3 data: "
                 << base::HexEncode(ac3_data.data(), ac3_data.size());
    return 0;
  }
  return kAc3NumChannelsTable[audio_coding_mode] + (lfe_channel_on ? 1 : 0);
}

}  // namespace media
}  // namespace shaka
