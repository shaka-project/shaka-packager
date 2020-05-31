#include "packager/media/formats/mp2t/mpeg1_header.h"

#include "packager/media/base/bit_reader.h"
#include "packager/media/base/bit_writer.h"
#include "packager/media/formats/mp2t/mp2t_common.h"

namespace {
const size_t kMpeg1HeaderMinSize = 4;
//const size_t kMpeg1HeaderCrcSize = 2;

const int kMpeg1V_INV = 0b01; /* Invalid version */
// const int kMpeg1V_2_5 = 0b00;
// const int kMpeg1V_2 = 0b10; /* MPEG Version 2 (ISO/IEC 13818-3) */
// const int kMpeg1V_1 = 0b11; /* MPEG Version 1 (ISO/IEC 11172-3) */

const int kMpeg1L_INV = 0b00; /* Invalid layer */
const int kMpeg1L_3 = 0b01;
const int kMpeg1L_2 = 0b10;
const int kMpeg1L_1 = 0b11;

const int kMpeg1SamplesPerFrameTable[] = {
  /* L1   L2   L3 */
  384, 1152, 1152 };
const size_t kMpeg1SamplesPerFrameTableSize = arraysize(kMpeg1SamplesPerFrameTable);

const int kMpeg1SampleRateTable[][3] = {
        /*        V1          V2        V2.5 */
        {      44100,      22050,      11025 },
        {      48000,      24000,      12000 },
        {      32000,      16000,       8000 }};
const size_t kMpeg1SampleRateTableSize = arraysize(kMpeg1SampleRateTable);

const int kMpeg1BitrateTable[][5] = {
        // V1:L1    V1:L2     V1:L3     V2:L1   V2:L2 & L3
        {       0,       0,       0,       0,       0 },
        {      32,      32,      32,      32,       8 },
        {      64,      48,      40,      48,      16 },
        {      96,      56,      48,      56,      24 },
        {     128,      64,      56,      64,      32 },
        {     160,      80,      64,      80,      40 },
        {     192,      96,      80,      96,      48 },
        {     224,     112,      96,     112,      56 },
        {     256,     128,     112,     128,      64 },
        {     288,     160,     128,     144,      80 },
        {     320,     192,     160,     160,      96 },
        {     352,     224,     192,     176,     112 },
        {     384,     256,     224,     192,     128 },
        {     416,     320,     256,     224,     144 },
        {     448,     384,     320,     256,     160 }};
const size_t kMpeg1BitrateTableSize = arraysize(kMpeg1BitrateTable);

}  // namespace

namespace shaka {
namespace media {
namespace mp2t {

bool Mpeg1Header::IsSyncWord(const uint8_t* buf) const {
  return (buf[0] == 0xff)
    && ((buf[1] & 0b11100000) == 0b11100000)
    && ((buf[1] & 0b00011000) != 0b00001000)
    && ((buf[1] & 0b00000110) != 0b00000000);
}

size_t Mpeg1Header::GetMinFrameSize() const {
  return kMpeg1HeaderMinSize + 1;
}

static inline size_t
Mpeg1SampleRate(uint8_t sr_idx, int version)
{
  static int sr_version_indexes[] = {2, -1, 1, 0};
  CHECK(version != 1);
  DCHECK_LT(sr_idx, kMpeg1SampleRateTableSize);
  return kMpeg1SampleRateTable[sr_idx][sr_version_indexes[version]];
}

static inline size_t
Mpeg1BitRate(uint8_t btr_idx, int version, int layer)
{
  static int btr_version_indexes[] = {2, -1, 1, 0};
  static int btr_layer_indexes[] = {-1, 2, 1, 0};

  assert(version != 1);
  assert(layer != 0);
  int vidx = btr_version_indexes[version];
  int lidx = btr_layer_indexes[layer];
  if (vidx == 2)
    vidx = 1;
  if (vidx == 1 && lidx > 1)
    lidx = 1;
  assert(btr_idx < 15);
  assert(vidx * 3 + lidx < 5);

  DCHECK_LT(btr_idx, kMpeg1BitrateTableSize);
  return kMpeg1BitrateTable[btr_idx][vidx*3+lidx] * 1000;
}

static inline size_t
Mpeg1FrameSize(int layer, int bitrate, int sample_rate, uint8_t padded)
{
  RCHECK(sample_rate > 0);
  if (layer == kMpeg1L_1)
    return (12 * bitrate / sample_rate + padded) * 4;
  return 144 * bitrate / sample_rate + padded;
}

size_t Mpeg1Header::GetSamplesPerFrame() const {
  RCHECK((layer_ > 0) && (layer_ <= kMpeg1SamplesPerFrameTableSize));
  return kMpeg1SamplesPerFrameTable[layer_ - 1];
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
  bitrate_ = Mpeg1BitRate(btr_idx, version_, layer_);

  uint8_t sr_idx;
  RCHECK(frame.ReadBits(2, &sr_idx));
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
  DCHECK_GT(num_bytes, static_cast<size_t>(1));
  uint8_t version = (data[1] & 0b00011000) >> 3;
  uint8_t layer = (data[1] & 0b00000110) >> 1;
  uint8_t btr_idx = (data[2] & 0b11110000) >> 4;
  uint8_t sr_idx = (data[2] & 0b00001100) >> 2;
  uint8_t padded = (data[2] & 0b00000010) >> 1;

  int bitrate = Mpeg1BitRate(btr_idx, version, layer);
  int samplerate = Mpeg1SampleRate(sr_idx, version);
  return Mpeg1FrameSize(layer, bitrate, samplerate, padded);
}

void Mpeg1Header::GetAudioSpecificConfig(std::vector<uint8_t>* buffer) const {
  // The following conversion table is extracted from ISO 14496 Part 3 -
  // Table 1.16 - Sampling Frequency Index.
  static const int kConfigFrequencyTable[] = {
    96000, 88200, 64000, 48000, 44100,
    32000, 24000, 22050, 16000, 12000,
    11025, 8000,  7350};
  static const size_t kConfigFrequencyTableSize = arraysize(kConfigFrequencyTable);
  uint8_t cft_idx;

  for (cft_idx = 0; cft_idx < kConfigFrequencyTableSize; cft_idx++)
    if (sample_rate_ == kConfigFrequencyTable[cft_idx])
      break;

  DCHECK(buffer);
  buffer->clear();
  BitWriter config(buffer);
  config.WriteBits(GetObjectType(), 5);
  config.WriteBits(cft_idx, 4);
  /*
   * NOTE: Number of channels matches channel_configuration index,
   * since mpeg1 has only 1 or 2 channels
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
  else if (layer_ == kMpeg1L_2)
    return 33;

  RCHECK(layer_ == kMpeg1L_3);
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
