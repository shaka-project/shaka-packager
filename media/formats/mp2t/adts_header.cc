// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "adts_header.h"

#include "media/base/bit_reader.h"
#include "media/formats/mp2t/mp2t_common.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media {
namespace mp2t {

AdtsHeader::AdtsHeader()
    : valid_config_(false),
      profile_(0),
      sampling_frequency_index_(0),
      channel_configuration_(0) {}

size_t AdtsHeader::GetAdtsFrameSize(const uint8* data, size_t num_bytes) {
  if (num_bytes < 6)
    return 0;
  return ((static_cast<int>(data[5]) >> 5) |
          (static_cast<int>(data[4]) << 3) |
          ((static_cast<int>(data[3]) & 0x3) << 11));
}

size_t AdtsHeader::GetAdtsHeaderSize(const uint8* data, size_t num_bytes) {
  if (num_bytes < 2)
    return 0;
  if (data[1] & 0x01)
    return kAdtsHeaderMinSize;
  return kAdtsHeaderMinSize + sizeof(uint16);  // Header + CRC.
}

bool AdtsHeader::Parse(
    const uint8* adts_frame, size_t adts_frame_size) {
  CHECK(adts_frame);

  valid_config_ = false;

  BitReader frame(adts_frame, adts_frame_size);
  // Verify frame starts with sync bits (0xfff).
  uint32 sync;
  RCHECK(frame.ReadBits(12, &sync));
  RCHECK(sync == 0xfff);
  // Skip MPEG version and layer.
  RCHECK(frame.SkipBits(3));
  // Get "protection absent" flag.
  bool protection_absent;
  RCHECK(frame.ReadBits(1, &protection_absent));
  // Get profile.
  RCHECK(frame.ReadBits(2, &profile_));
  // Get sampling frequency.
  RCHECK(frame.ReadBits(4, &sampling_frequency_index_));
  RCHECK(sampling_frequency_index_ < kAdtsFrequencyTableSize);
  // Skip private stream bit.
  RCHECK(frame.SkipBits(1));
  // Get number of audio channels.
  RCHECK(frame.ReadBits(3, &channel_configuration_));
  RCHECK((channel_configuration_ > 0) &&
         (channel_configuration_ < kAdtsNumChannelsTableSize));
  // Skip originality, home and copyright info.
  RCHECK(frame.SkipBits(4));
  // Verify that the frame size matches input parameters.
  uint16 frame_size;
  RCHECK(frame.ReadBits(13, &frame_size));
  RCHECK(frame_size == adts_frame_size);
  // Skip buffer fullness indicator.
  RCHECK(frame.SkipBits(11));
  uint8 num_blocks_minus_1;
  RCHECK(frame.ReadBits(2, &num_blocks_minus_1));
  if (num_blocks_minus_1) {
    NOTIMPLEMENTED() << "ADTS frames with more than one data block "
                        "not supported.";
    return false;
  }

  valid_config_ = true;
  return true;
}

bool AdtsHeader::GetAudioSpecificConfig(
    std::vector<uint8>* buffer) const {
  DCHECK(buffer);
  if (!valid_config_)
    return false;

  buffer->resize(2);
  (*buffer)[0] = ((profile_ + 1) << 3) | (sampling_frequency_index_ >> 1);
  (*buffer)[1] = ((sampling_frequency_index_ & 1) << 7) |
      (channel_configuration_ << 3);
  return true;
}

uint8 AdtsHeader::GetObjectType() const {
  return profile_ + 1;
}

uint32 AdtsHeader::GetSamplingFrequency() const {
  DCHECK_LT(sampling_frequency_index_, kAdtsFrequencyTableSize);
  return kAdtsFrequencyTable[sampling_frequency_index_];
}

uint8 AdtsHeader::GetNumChannels() const {
  DCHECK_GT(channel_configuration_, 0);
  DCHECK_LT(channel_configuration_, kAdtsNumChannelsTableSize);
  return kAdtsNumChannelsTable[channel_configuration_];
}

}  // namespace mp2t
}  // namespace media
