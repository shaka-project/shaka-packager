// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp2t/mpeg1_header.h>

#include <absl/log/check.h>

#include <packager/media/base/bit_reader.h>
#include <packager/media/base/bit_writer.h>
#include <packager/media/formats/mp2t/mp2t_common.h>

// Parsing is done according to
// https://www.datavoyage.com/mpgscript/mpeghdr.htm
namespace {
const size_t kMpeg1HeaderMinSize = 4;

const uint8_t kMpeg1V_INV = 0b01; /* Invalid version */
const uint8_t kMpeg1L_INV = 0b00; /* Invalid layer */

const uint8_t kMpeg1L_3 = 0b01;
const uint8_t kMpeg1L_2 = 0b10;
const uint8_t kMpeg1L_1 = 0b11;

const size_t kMpeg1SamplesPerFrameTable[] = {
    // L1, L2, L3
    384, 1152, 1152};

const uint32_t kMpeg1SampleRateTable[][3] = {
    // clang-format off
    //  V1,    V2,  V2.5
    {44100, 22050, 11025},
    {48000, 24000, 12000},
    {32000, 16000, 8000}};
    // clang-format on
const size_t kMpeg1SampleRateTableSize = std::size(kMpeg1SampleRateTable);

static inline uint32_t Mpeg1SampleRate(uint8_t sr_idx, uint8_t version) {
  static int sr_version_indexes[] = {2, -1, 1, 0};  // {V2.5, RESERVED, V2, V1}
  DCHECK_NE(version, 1);
  DCHECK_LT(sr_idx, kMpeg1SampleRateTableSize);
  return kMpeg1SampleRateTable[sr_idx][sr_version_indexes[version]];
}

const uint32_t kMpeg1BitrateTable[][5] = {
    // clang-format off
    // V1:L1,  V1:L2, V1:L3, V2:L1, V2&V2.5:L2&L3
    {      0,     0,     0,     0,     0},
    {     32,    32,    32,    32,     8},
    {     64,    48,    40,    48,    16},
    {     96,    56,    48,    56,    24},
    {     128,   64,    56,    64,    32},
    {     160,   80,    64,    80,    40},
    {     192,   96,    80,    96,    48},
    {     224,  112,    96,   112,    56},
    {     256,  128,   112,   128,    64},
    {     288,  160,   128,   144,    80},
    {     320,  192,   160,   160,    96},
    {     352,  224,   192,   176,   112},
    {     384,  256,   224,   192,   128},
    {     416,  320,   256,   224,   144},
    {     448,  384,   320,   256,   160}};
    // clang-format on
const size_t kMpeg1BitrateTableSize = std::size(kMpeg1BitrateTable);

static inline uint32_t Mpeg1BitRate(uint8_t btr_idx,
                                    uint8_t version,
                                    uint8_t layer) {
  static int btr_version_indexes[] = {1, -1, 1, 0};  // {V2.5, RESERVED, V2, V1}
  static int btr_layer_indexes[] = {-1, 2, 1, 0};    // {RESERVED, L3, L2, L1}

  DCHECK_NE(version, 1);
  DCHECK_NE(layer, 0);
  int vidx = btr_version_indexes[version];
  int lidx = btr_layer_indexes[layer];
  if (vidx == 1 && lidx > 1)
    lidx = 1;

  DCHECK_LT(vidx * 3 + lidx, 5);
  DCHECK_LT(btr_idx, kMpeg1BitrateTableSize);
  return kMpeg1BitrateTable[btr_idx][vidx * 3 + lidx] * 1000;
}

static inline size_t Mpeg1FrameSize(uint8_t layer,
                                    uint32_t bitrate,
                                    uint32_t sample_rate,
                                    uint8_t padded) {
  DCHECK_GT(sample_rate, static_cast<uint32_t>(0));
  if (layer == kMpeg1L_1)
    return (12 * bitrate / sample_rate + padded) * 4;
  return 144 * bitrate / sample_rate + padded;
}

}  // namespace

namespace shaka {
namespace media {
namespace mp2t {

bool Mpeg1Header::IsSyncWord(const uint8_t* buf) const {
  return (buf[0] == 0xff) &&
         ((buf[1] & 0b11100000) == 0b11100000)
         // Version 01 is reserved
         && ((buf[1] & 0b00011000) != 0b00001000)
         // Layer 00 is reserved
         && ((buf[1] & 0b00000110) != 0b00000000);
}

size_t Mpeg1Header::GetMinFrameSize() const {
  return kMpeg1HeaderMinSize + 1;
}

size_t Mpeg1Header::GetSamplesPerFrame() const {
  static int spf_layer_indexes[] = {-1, 2, 1, 0};  // {RESERVED, L3, L2, L1}
  DCHECK_NE(layer_, 0);
  return kMpeg1SamplesPerFrameTable[spf_layer_indexes[layer_]];
}

bool Mpeg1Header::Parse(const uint8_t* mpeg1_frame, size_t mpeg1_frame_size) {
  DCHECK(mpeg1_frame);

  if (mpeg1_frame_size < kMpeg1HeaderMinSize)
    return false;

  BitReader frame(mpeg1_frame, mpeg1_frame_size);
  // Verify frame starts with sync bits (0x7ff).
  uint32_t sync;
  RCHECK(frame.ReadBits(11, &sync));
  RCHECK(sync == 0x7ff);
  // MPEG version and layer.
  RCHECK(frame.ReadBits(2, &version_));
  RCHECK(version_ != kMpeg1V_INV);
  RCHECK(frame.ReadBits(2, &layer_));
  RCHECK(layer_ != kMpeg1L_INV);
  RCHECK(frame.ReadBits(1, &protection_absent_));

  uint8_t btr_idx;
  RCHECK(frame.ReadBits(4, &btr_idx));
  RCHECK(btr_idx > 0);
  bitrate_ = Mpeg1BitRate(btr_idx, version_, layer_);

  uint8_t sr_idx;
  RCHECK(frame.ReadBits(2, &sr_idx));
  RCHECK(sr_idx < 0b11);
  sample_rate_ = Mpeg1SampleRate(sr_idx, version_);

  RCHECK(frame.ReadBits(1, &padded_));
  // Skip private stream bit.
  RCHECK(frame.SkipBits(1));

  RCHECK(frame.ReadBits(2, &channel_mode_));
  // Skip Mode extension
  RCHECK(frame.SkipBits(2));
  // Skip copyright, origination and emphasis info.
  RCHECK(frame.SkipBits(4));

  return true;
}

size_t Mpeg1Header::GetHeaderSize() const {
  // Unlike ADTS, for MP3, the whole frame is included in the media sample, so
  // return 0 header size.
  return 0;
}

size_t Mpeg1Header::GetFrameSize() const {
  return Mpeg1FrameSize(layer_, bitrate_, sample_rate_, padded_);
}

size_t Mpeg1Header::GetFrameSizeWithoutParsing(const uint8_t* data,
                                               size_t num_bytes) const {
  DCHECK_GT(num_bytes, static_cast<size_t>(2));
  uint8_t version = (data[1] & 0b00011000) >> 3;
  uint8_t layer = (data[1] & 0b00000110) >> 1;
  uint8_t btr_idx = (data[2] & 0b11110000) >> 4;
  uint8_t sr_idx = (data[2] & 0b00001100) >> 2;
  uint8_t padded = (data[2] & 0b00000010) >> 1;

  if ((version == kMpeg1V_INV) || (layer == kMpeg1L_INV) || (btr_idx == 0) ||
      (sr_idx == 0b11))
    return 0;

  uint32_t bitrate = Mpeg1BitRate(btr_idx, version, layer);
  uint32_t samplerate = Mpeg1SampleRate(sr_idx, version);
  return Mpeg1FrameSize(layer, bitrate, samplerate, padded);
}

void Mpeg1Header::GetAudioSpecificConfig(std::vector<uint8_t>* buffer) const {
  // The following conversion table is extracted from ISO 14496 Part 3 -
  // Table 1.16 - Sampling Frequency Index.
  static const size_t kConfigFrequencyTable[] = {
      96000, 88200, 64000, 48000, 44100, 32000, 24000,
      22050, 16000, 12000, 11025, 8000,  7350};
  static const size_t kConfigFrequencyTableSize =
      std::size(kConfigFrequencyTable);
  uint8_t cft_idx;

  for (cft_idx = 0; cft_idx < kConfigFrequencyTableSize; cft_idx++)
    if (sample_rate_ == kConfigFrequencyTable[cft_idx])
      break;

  DCHECK(buffer);
  buffer->clear();
  BitWriter config(buffer);

  // ISO/IEC 14496:3 Table 1.16 Syntax of GetAudioObjetType()
  auto object_type = GetObjectType();
  if (object_type <= 31) {
    config.WriteBits(object_type, 5);
  } else {
    config.WriteBits(31, 5);
    config.WriteBits(object_type - 32, 6);
  }

  config.WriteBits(cft_idx, 4);
  /*
   * NOTE: Number of channels matches channel_configuration index,
   * since mpeg1 audio has only 1 or 2 channels
   */
  config.WriteBits(GetNumChannels(), 4);
  config.Flush();
}

uint8_t Mpeg1Header::GetObjectType() const {
  /*
   * ISO14496-3:2009 Table 1.17 - Audio Object Types
   */
  if (layer_ == kMpeg1L_1)
    return 32;
  if (layer_ == kMpeg1L_2)
    return 33;

  DCHECK_EQ(layer_, kMpeg1L_3);
  return 34;
}

uint32_t Mpeg1Header::GetSamplingFrequency() const {
  return sample_rate_;
}

uint8_t Mpeg1Header::GetNumChannels() const {
  if (channel_mode_ == 0b11)
    return 1;
  return 2;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
