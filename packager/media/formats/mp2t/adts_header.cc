// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp2t/adts_header.h>

#include <absl/log/check.h>

#include <packager/macros/logging.h>
#include <packager/media/base/bit_reader.h>
#include <packager/media/base/bit_writer.h>
#include <packager/media/formats/mp2t/mp2t_common.h>

namespace {
const size_t kAdtsHeaderMinSize = 7;

// The following conversion table is extracted from ISO 14496 Part 3 -
// Table 1.16 - Sampling Frequency Index.
const int kAdtsFrequencyTable[] = {96000, 88200, 64000, 48000, 44100,
                                   32000, 24000, 22050, 16000, 12000,
                                   11025, 8000,  7350};
const size_t kAdtsFrequencyTableSize = std::size(kAdtsFrequencyTable);

// The following conversion table is extracted from ISO 14496 Part 3 -
// Table 1.17 - Channel Configuration.
const int kAdtsNumChannelsTable[] = {0, 1, 2, 3, 4, 5, 6, 8};
const size_t kAdtsNumChannelsTableSize = std::size(kAdtsNumChannelsTable);
}  // namespace

namespace shaka {
namespace media {
namespace mp2t {

bool AdtsHeader::IsSyncWord(const uint8_t* buf) const {
  return (buf[0] == 0xff) && ((buf[1] & 0xf6) == 0xf0);
}

size_t AdtsHeader::GetMinFrameSize() const {
  return kAdtsHeaderMinSize + 1;
}

size_t AdtsHeader::GetSamplesPerFrame() const {
  const size_t kSamplesPerAacFrame = 1024;
  return kSamplesPerAacFrame;
}

bool AdtsHeader::Parse(const uint8_t* adts_frame, size_t adts_frame_size) {
  CHECK(adts_frame);

  if (adts_frame_size < kAdtsHeaderMinSize)
    return false;

  BitReader frame(adts_frame, adts_frame_size);
  // Verify frame starts with sync bits (0xfff).
  uint32_t sync;
  RCHECK(frame.ReadBits(12, &sync));
  RCHECK(sync == 0xfff);
  // Skip MPEG version and layer.
  RCHECK(frame.SkipBits(3));
  RCHECK(frame.ReadBits(1, &protection_absent_));
  RCHECK(frame.ReadBits(2, &profile_));
  RCHECK(frame.ReadBits(4, &sampling_frequency_index_));
  RCHECK(sampling_frequency_index_ < kAdtsFrequencyTableSize);
  // Skip private stream bit.
  RCHECK(frame.SkipBits(1));
  RCHECK(frame.ReadBits(3, &channel_configuration_));
  RCHECK(channel_configuration_ < kAdtsNumChannelsTableSize);
  // Skip originality, home and copyright info.
  RCHECK(frame.SkipBits(4));
  RCHECK(frame.ReadBits(13, &frame_size_));
  // Skip buffer fullness indicator.
  RCHECK(frame.SkipBits(11));
  uint8_t num_blocks_minus_1;
  RCHECK(frame.ReadBits(2, &num_blocks_minus_1));
  if (num_blocks_minus_1) {
    NOTIMPLEMENTED() << "ADTS frames with more than one data block "
                        "not supported.";
    return false;
  }
  return true;
}

size_t AdtsHeader::GetHeaderSize() const {
  const size_t kCrcSize = sizeof(uint16_t);
  return kAdtsHeaderMinSize + (protection_absent_ ? 0 : kCrcSize);
}

size_t AdtsHeader::GetFrameSize() const {
  return frame_size_;
}

size_t AdtsHeader::GetFrameSizeWithoutParsing(const uint8_t* data,
                                              size_t num_bytes) const {
  DCHECK_GT(num_bytes, static_cast<size_t>(5));
  return ((static_cast<int>(data[5]) >> 5) | (static_cast<int>(data[4]) << 3) |
          ((static_cast<int>(data[3]) & 0x3) << 11));
}

void AdtsHeader::GetAudioSpecificConfig(std::vector<uint8_t>* buffer) const {
  DCHECK(buffer);
  buffer->clear();
  BitWriter config(buffer);
  config.WriteBits(GetObjectType(), 5);
  config.WriteBits(sampling_frequency_index_, 4);
  config.WriteBits(channel_configuration_, 4);
  config.Flush();
}

uint8_t AdtsHeader::GetObjectType() const {
  return profile_ + 1;
}

uint32_t AdtsHeader::GetSamplingFrequency() const {
  DCHECK_LT(sampling_frequency_index_, kAdtsFrequencyTableSize);
  return kAdtsFrequencyTable[sampling_frequency_index_];
}

uint8_t AdtsHeader::GetNumChannels() const {
  DCHECK_LT(channel_configuration_, kAdtsNumChannelsTableSize);
  return kAdtsNumChannelsTable[channel_configuration_];
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
