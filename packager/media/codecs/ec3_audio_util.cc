// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/ec3_audio_util.h>

#include <absl/log/check.h>
#include <absl/strings/escaping.h>

#include <packager/media/base/bit_reader.h>
#include <packager/media/base/rcheck.h>
#include <packager/utils/bytes_to_string_view.h>

namespace shaka {
namespace media {

namespace {

// Channels bit map. 16 bits.
// Bit,      Location
// 0(MSB),   Left
// 1,        Center
// 2,        Right
// 3,        Left Surround
// 4,        Right Surround
// 5,        Left center/Right center pair
// 6,        Left rear surround/Right rear surround pair
// 7,        Center surround
// 8,        Top center surround
// 9,        Left surround direct/Right surround direct pair
// 10,       Left wide/Right wide pair
// 11,       Lvertical height/Right vertical height pair
// 12,       Center vertical height
// 13,       Lts/Rts pair
// 14,       LFE2
// 15,       LFE
enum kEC3AudioChannelMap {
  kLeft = 0x8000,
  kCenter = 0x4000,
  kRight = 0x2000,
  kLeftSurround = 0x1000,
  kRightSurround = 0x800,
  kLcRcPair = 0x400,
  kLrsRrsPair = 0x200,
  kCenterSurround = 0x100,
  kTopCenterSurround = 0x80,
  kLsdRsdPair = 0x40,
  kLwRwPair = 0x20,
  kLvhRvhPair = 0x10,
  kCenterVerticalHeight = 0x8,
  kLtsRtsPair = 0x4,
  kLFE2 = 0x2,
  kLFEScreen = 0x1
};
// Number of channels for the channel bit above. The first entry corresponds to
// kLeft, which has one channel. All the XxxPairs bits have two channels.
const size_t kChannelCountArray[] = {
    1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2, 2, 1, 2, 1, 1,
};
static_assert(std::size(kChannelCountArray) == 16u,
              "Channel count array should have 16 entries.");

// EC3 Audio coding mode map (acmod) to determine EC3 audio channel layout. The
// value stands for the existence of Left, Center, Right, Left surround, and
// Right surround.
const uint16_t kEC3AudioCodingModeMap[] = {
    kLeft | kRight,
    kCenter,
    kLeft | kRight,
    kLeft | kCenter | kRight,
    kLeft | kRight | kLeftSurround | kRightSurround,
    kLeft | kCenter | kRight | kLeftSurround | kRightSurround,
    kLeft | kRight | kLeftSurround | kRightSurround,
    kLeft | kCenter | kRight | kLeftSurround | kRightSurround,
};

// Reverse bit order.
uint8_t ReverseBits8(uint8_t n) {
  n = ((n >> 1) & 0x55) | ((n & 0x55) << 1);
  n = ((n >> 2) & 0x33) | ((n & 0x33) << 2);
  return ((n >> 4) & 0x0f) | ((n & 0x0f) << 4);
}

// Mapping of channel configurations to the MPEG audio value based on
// ETSI TS 102 366 V1.4.1 Digital Audio Compression (AC-3, Enhanced AC-3)
// Standard Table I.1.1
uint32_t EC3ChannelMaptoMPEGValue(uint32_t channel_map) {
  uint32_t ret = 0;

  switch (channel_map) {
    case kCenter:
      ret = 1;
      break;
    case kLeft | kRight:
      ret = 2;
      break;
    case kCenter| kLeft | kRight:
      ret = 3;
      break;
    case kCenter | kLeft | kRight | kCenterSurround:
      ret = 4;
      break;
    case kCenter | kLeft | kRight | kLeftSurround | kRightSurround:
      ret = 5;
      break;
    case kCenter | kLeft | kRight | kLeftSurround | kRightSurround |
         kLFEScreen:
      ret = 6;
      break;
    case kCenter | kLeft | kRight | kLwRwPair | kLeftSurround | kRightSurround |
         kLFEScreen:
      ret = 7;
      break;
    case kLeft | kRight | kCenterSurround:
      ret = 9;
      break;
    case kLeft | kRight | kLeftSurround | kRightSurround:
      ret = 10;
      break;
    case kCenter | kLeft | kRight | kLrsRrsPair | kCenterSurround | kLFEScreen:
      ret = 11;
      break;
    case kCenter | kLeft | kRight | kLeftSurround | kRightSurround |
         kLrsRrsPair | kLFEScreen:
      ret = 12;
      break;
    case kCenter | kLeft | kRight | kLeftSurround | kRightSurround |
         kLFEScreen | kLvhRvhPair:
      ret = 14;
      break;
    case kCenter | kLeft | kRight | kLeftSurround | kRightSurround |
         kLFEScreen | kLvhRvhPair | kLtsRtsPair:
      ret = 16;
      break;
    case kCenter | kLeft | kRight | kLeftSurround | kRightSurround |
         kLFEScreen | kLvhRvhPair | kCenterVerticalHeight | kLtsRtsPair |
         kTopCenterSurround:
      ret = 17;
      break;
    case kCenter | kLeft | kRight | kLsdRsdPair | kLrsRrsPair | kLFEScreen |
         kLvhRvhPair | kLtsRtsPair:
      ret = 19;
      break;
    default:
      ret = 0xFFFFFFFF;
  }
  return ret;
}

bool ExtractEc3Data(const std::vector<uint8_t>& ec3_data,
                    uint8_t* audio_coding_mode,
                    bool* lfe_channel_on,
                    uint16_t* dependent_substreams_layout,
                    uint32_t* ec3_joc_complexity) {
  BitReader bit_reader(ec3_data.data(), ec3_data.size());
  // Read number of independent substreams and parse the independent substreams.
  uint8_t number_independent_substreams;
  RCHECK(bit_reader.SkipBits(13) &&
         bit_reader.ReadBits(3, &number_independent_substreams));
  // The value of this field is one less than the number of independent
  // substreams present.
  ++number_independent_substreams;

  // Parse audio_coding_mode, dependent_substreams_layout and lfe_channel_on
  // from the first independent substream.
  // Independent substream in EC3Specific box:
  // fscod: 2 bits
  // bsid: 5 bits
  // reserved_1: 1 bit
  // asvc: 1 bit
  // bsmod: 3 bits
  // acmod: 3 bits
  // lfeon: 1 bit
  // reserved_2: 3 bits
  // num_dep_sub: 4 bits
  // If num_dep_sub > 0, chan_loc is present and the size is 9 bits.
  // Otherwise, reserved_3 is present and the size is 1 bit.
  // chan_loc: 9 bits
  // reserved_3: 1 bit
  RCHECK(bit_reader.SkipBits(12));
  RCHECK(bit_reader.ReadBits(3, audio_coding_mode));
  RCHECK(bit_reader.ReadBits(1, lfe_channel_on));

  uint8_t number_dependent_substreams = 0;
  RCHECK(bit_reader.SkipBits(3));
  RCHECK(bit_reader.ReadBits(4, &number_dependent_substreams));

  *dependent_substreams_layout = 0;
  if (number_dependent_substreams > 0) {
    RCHECK(bit_reader.ReadBits(9, dependent_substreams_layout));
  } else {
    RCHECK(bit_reader.SkipBits(1));
  }
  *ec3_joc_complexity = 0;
  if (bit_reader.bits_available() < 16) {
    return true;
  }

  RCHECK(bit_reader.SkipBits(7));
  bool ec3_joc_flag;
  RCHECK(bit_reader.ReadBits(1, &ec3_joc_flag));
  if (ec3_joc_flag) {
    RCHECK(bit_reader.ReadBits(8, ec3_joc_complexity));
  }
  return true;
}

}  // namespace

bool CalculateEC3ChannelMap(const std::vector<uint8_t>& ec3_data,
                            uint32_t* channel_map) {
  uint8_t audio_coding_mode;
  bool lfe_channel_on;
  uint16_t dependent_substreams_layout;
  uint32_t ec3_joc_complexity;
  if (!ExtractEc3Data(ec3_data, &audio_coding_mode, &lfe_channel_on,
                      &dependent_substreams_layout, &ec3_joc_complexity)) {
    LOG(WARNING) << "Seeing invalid EC3 data: "
                 << absl::BytesToHexString(
                        byte_vector_to_string_view(ec3_data));
    return false;
  }

  // Dependent substreams layout bit map:
  // Bit,    Location
  // 0,      Lc/Rc pair
  // 1,      Lrs/Rrs pair
  // 2,      Cs
  // 3,      Ts
  // 4,      Lsd/Rsd pair
  // 5,      Lw/Rw pair
  // 6,      Lvh/Rvh pair
  // 7,      Cvh
  // 8(MSB), LFE2
  // Reverse bit order of dependent substreams channel layout (LFE2 not
  // included) to apply on channel_map bit 5 - 12.
  const uint8_t reversed_dependent_substreams_layout =
      ReverseBits8(dependent_substreams_layout & 0xFF);

  *channel_map = kEC3AudioCodingModeMap[audio_coding_mode] |
                 (reversed_dependent_substreams_layout << 3);
  if (dependent_substreams_layout & 0x100)
    *channel_map |= kLFE2;
  if (lfe_channel_on)
    *channel_map |= kLFEScreen;
  return true;
}

bool CalculateEC3ChannelMPEGValue(const std::vector<uint8_t>& ec3_data,
                                  uint32_t* ec3_channel_mpeg_value) {
  uint32_t channel_map;
  if (!CalculateEC3ChannelMap(ec3_data, &channel_map))
    return false;
  *ec3_channel_mpeg_value = EC3ChannelMaptoMPEGValue(channel_map);
  return true;
}

size_t GetEc3NumChannels(const std::vector<uint8_t>& ec3_data) {
  uint32_t channel_map;
  if (!CalculateEC3ChannelMap(ec3_data, &channel_map))
    return 0;

  size_t num_channels = 0;
  int bit = kLeft;
  for (size_t channel_count : kChannelCountArray) {
    if (channel_map & bit)
      num_channels += channel_count;
    bit >>= 1;
  }
  DCHECK_EQ(bit, 0);
  return num_channels;
}

bool GetEc3JocComplexity(const std::vector<uint8_t>& ec3_data,
                         uint32_t* ec3_joc_complexity) {
  uint8_t audio_coding_mode;
  bool lfe_channel_on;
  uint16_t dependent_substreams_layout;

  if (!ExtractEc3Data(ec3_data, &audio_coding_mode, &lfe_channel_on,
                      &dependent_substreams_layout, ec3_joc_complexity)) {
    LOG(WARNING) << "Seeing invalid EC3 data: "
                 << absl::BytesToHexString(
                        byte_vector_to_string_view(ec3_data));
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace shaka
