// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp2t/ac3_header.h>

#include <absl/log/check.h>

#include <packager/media/base/bit_reader.h>
#include <packager/media/base/bit_writer.h>
#include <packager/media/formats/mp2t/mp2t_common.h>

namespace shaka {
namespace media {
namespace mp2t {
namespace {

// ASTC Standard A/52:2012 Table 5.6 Sample Rate Codes.
const uint32_t kAc3SampleRateTable[] = {48000, 44100, 32000};

// ASTC Standard A/52:2012 Table 5.8 Audio Coding Mode.
const uint8_t kAc3NumChannelsTable[] = {2, 1, 2, 3, 3, 4, 4, 5};

// ATSC Standard A/52:2012 Table 5.18 Frame Size Code Table
// (in words = 16 bits).
const size_t kFrameSizeCodeTable[][3] = {
    // {32kHz, 44.1kHz, 48kHz}
    {96, 69, 64},       {96, 70, 64},       {120, 87, 80},
    {120, 88, 80},      {144, 104, 96},     {144, 105, 96},
    {168, 121, 112},    {168, 122, 112},    {192, 139, 128},
    {192, 140, 128},    {240, 174, 160},    {240, 175, 160},
    {288, 208, 192},    {288, 209, 192},    {336, 243, 224},
    {336, 244, 224},    {384, 278, 256},    {384, 279, 256},
    {480, 348, 320},    {480, 349, 320},    {576, 417, 384},
    {576, 418, 384},    {672, 487, 448},    {672, 488, 448},
    {768, 557, 512},    {768, 558, 512},    {960, 696, 640},
    {960, 697, 640},    {1152, 835, 768},   {1152, 836, 768},
    {1344, 975, 896},   {1344, 976, 896},   {1536, 1114, 1024},
    {1536, 1115, 1024}, {1728, 1253, 1152}, {1728, 1254, 1152},
    {1920, 1393, 1280}, {1920, 1394, 1280},
};

// Calculate the size of the frame from the sample rate code and the
// frame size code.
// @return the size of the frame (header + payload).
size_t CalcFrameSize(uint8_t fscod, uint8_t frmsizecod) {
  const size_t kNumFscode = std::size(kAc3SampleRateTable);
  DCHECK_LT(fscod, kNumFscode);
  DCHECK_LT(frmsizecod, std::size(kFrameSizeCodeTable));
  // The order of frequencies are reversed in |kFrameSizeCodeTable| compared to
  // |kAc3SampleRateTable|.
  const int index = kNumFscode - 1 - fscod;
  return kFrameSizeCodeTable[frmsizecod][index] * 2;
}

}  // namespace

bool Ac3Header::IsSyncWord(const uint8_t* buf) const {
  DCHECK(buf);
  // ATSC Standard A/52:2012 5.4.1 syncinfo: Synchronization Information.
  return buf[0] == 0x0B && buf[1] == 0x77;
}

size_t Ac3Header::GetMinFrameSize() const {
  // Arbitrary. Actual frame size starts with 96 words.
  const size_t kMinAc3FrameSize = 10u;
  return kMinAc3FrameSize;
}

size_t Ac3Header::GetSamplesPerFrame() const {
  // ATSC Standard A/52:2012
  // Annex A: AC-3 Elementary Streams in the MPEG-2 Multiplex.
  const size_t kSamplesPerAc3Frame = 1536;
  return kSamplesPerAc3Frame;
}

bool Ac3Header::Parse(const uint8_t* audio_frame, size_t audio_frame_size) {
  BitReader frame(audio_frame, audio_frame_size);

  // ASTC Standard A/52:2012 5. BIT STREAM SYNTAX.
  // syncinfo: synchronization information section.
  uint16_t syncword;
  RCHECK(frame.ReadBits(16, &syncword));
  RCHECK(syncword == 0x0B77);
  uint16_t crc1;
  RCHECK(frame.ReadBits(16, &crc1));
  RCHECK(frame.ReadBits(2, &fscod_));
  RCHECK(fscod_ < std::size(kAc3SampleRateTable));
  RCHECK(frame.ReadBits(6, &frmsizecod_));
  RCHECK(frmsizecod_ < std::size(kFrameSizeCodeTable));

  // bsi: bit stream information section.
  RCHECK(frame.ReadBits(5, &bsid_));
  RCHECK(frame.ReadBits(3, &bsmod_));

  RCHECK(frame.ReadBits(3, &acmod_));
  RCHECK(acmod_ < std::size(kAc3NumChannelsTable));
  // If 3 front channels.
  if ((acmod_ & 0x01) && (acmod_ != 0x01))
    RCHECK(frame.SkipBits(2));  // cmixlev.
  // If a surround channel exists.
  if (acmod_ & 0x04)
    RCHECK(frame.SkipBits(2));  // surmixlev.
  // If in 2/0 mode.
  if (acmod_ == 0x02)
    RCHECK(frame.SkipBits(2));  // dsurmod.

  RCHECK(frame.ReadBits(1, &lfeon_));

  return true;
}

size_t Ac3Header::GetHeaderSize() const {
  // Unlike ADTS, for AC3, the whole frame is included in the media sample, so
  // return 0 header size.
  return 0;
}

size_t Ac3Header::GetFrameSize() const {
  return CalcFrameSize(fscod_, frmsizecod_);
}

size_t Ac3Header::GetFrameSizeWithoutParsing(const uint8_t* data,
                                             size_t num_bytes) const {
  DCHECK_GT(num_bytes, static_cast<size_t>(4));
  uint8_t fscod = data[4] >> 6;
  uint8_t frmsizecod = data[4] & 0x3f;
  return CalcFrameSize(fscod, frmsizecod);
}

void Ac3Header::GetAudioSpecificConfig(std::vector<uint8_t>* buffer) const {
  DCHECK(buffer);
  buffer->clear();
  BitWriter config(buffer);
  // Accoding to ETSI TS 102 366 V1.3.1 (2014-08) F.4 AC3SpecificBox.
  config.WriteBits(fscod_, 2);
  config.WriteBits(bsid_, 5);
  config.WriteBits(bsmod_, 3);
  config.WriteBits(acmod_, 3);
  config.WriteBits(lfeon_, 1);
  const uint8_t bit_rate_code = frmsizecod_ >> 1;
  config.WriteBits(bit_rate_code, 5);
  config.Flush();
}

uint8_t Ac3Header::GetObjectType() const {
  // Only useful for AAC. Return a dummy value instead.
  return 0;
}

uint32_t Ac3Header::GetSamplingFrequency() const {
  DCHECK_LT(fscod_, std::size(kAc3SampleRateTable));
  return kAc3SampleRateTable[fscod_];
}

uint8_t Ac3Header::GetNumChannels() const {
  DCHECK_LT(acmod_, std::size(kAc3NumChannelsTable));
  return kAc3NumChannelsTable[acmod_] + (lfeon_ ? 1 : 0);
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
