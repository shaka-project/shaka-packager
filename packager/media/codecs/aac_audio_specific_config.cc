// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/codecs/aac_audio_specific_config.h"

#include <algorithm>

#include "packager/base/logging.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/rcheck.h"

namespace {

// Sampling Frequency Index table, from ISO 14496-3 Table 1.16
static const uint32_t kSampleRates[] = {96000, 88200, 64000, 48000, 44100,
                                        32000, 24000, 22050, 16000, 12000,
                                        11025, 8000,  7350};

// Channel Configuration table, from ISO 14496-3 Table 1.17
const uint8_t kChannelConfigs[] = {0, 1, 2, 3, 4, 5, 6, 8};

}  // namespace

namespace shaka {
namespace media {

AACAudioSpecificConfig::AACAudioSpecificConfig() {}

AACAudioSpecificConfig::~AACAudioSpecificConfig() {}

bool AACAudioSpecificConfig::Parse(const std::vector<uint8_t>& data) {
  if (data.empty())
    return false;

  BitReader reader(&data[0], data.size());
  uint8_t extension_type = AOT_NULL;
  uint8_t extension_frequency_index = 0xff;

  sbr_present_ = false;
  ps_present_ = false;
  frequency_ = 0;
  extension_frequency_ = 0;

  // The following code is written according to ISO 14496 Part 3 Table 1.13 -
  // Syntax of AudioSpecificConfig.

  // Read base configuration.
  // Audio Object Types specified in ISO 14496-3, Table 1.15.
  RCHECK(reader.ReadBits(5, &audio_object_type_));
  // Audio objects type >=31 is not supported yet.
  RCHECK(audio_object_type_ < 31);
  RCHECK(reader.ReadBits(4, &frequency_index_));
  if (frequency_index_ == 0xf)
    RCHECK(reader.ReadBits(24, &frequency_));
  RCHECK(reader.ReadBits(4, &channel_config_));

  // Read extension configuration.
  if (audio_object_type_ == AOT_SBR || audio_object_type_ == AOT_PS) {
    sbr_present_ = audio_object_type_ == AOT_SBR;
    ps_present_ = audio_object_type_ == AOT_PS;
    extension_type = AOT_SBR;
    RCHECK(reader.ReadBits(4, &extension_frequency_index));
    if (extension_frequency_index == 0xf)
      RCHECK(reader.ReadBits(24, &extension_frequency_));
    RCHECK(reader.ReadBits(5, &audio_object_type_));
    // Audio objects type >=31 is not supported yet.
    RCHECK(audio_object_type_ < 31);
  }

  RCHECK(SkipDecoderGASpecificConfig(&reader));
  RCHECK(SkipErrorSpecificConfig());

  // Read extension configuration again
  // Note: The check for 16 available bits comes from the AAC spec.
  if (extension_type != AOT_SBR && reader.bits_available() >= 16) {
    uint16_t sync_extension_type;
    uint8_t sbr_present_flag;
    uint8_t ps_present_flag;

    if (reader.ReadBits(11, &sync_extension_type) &&
        sync_extension_type == 0x2b7) {
      if (reader.ReadBits(5, &extension_type) && extension_type == 5) {
        RCHECK(reader.ReadBits(1, &sbr_present_flag));
        sbr_present_ = sbr_present_flag != 0;

        if (sbr_present_flag) {
          RCHECK(reader.ReadBits(4, &extension_frequency_index));

          if (extension_frequency_index == 0xf)
            RCHECK(reader.ReadBits(24, &extension_frequency_));

          // Note: The check for 12 available bits comes from the AAC spec.
          if (reader.bits_available() >= 12) {
            RCHECK(reader.ReadBits(11, &sync_extension_type));
            if (sync_extension_type == 0x548) {
              RCHECK(reader.ReadBits(1, &ps_present_flag));
              ps_present_ = ps_present_flag != 0;
            }
          }
        }
      }
    }
  }

  if (frequency_ == 0) {
    RCHECK(frequency_index_ < arraysize(kSampleRates));
    frequency_ = kSampleRates[frequency_index_];
  }

  if (extension_frequency_ == 0 && extension_frequency_index != 0xff) {
    RCHECK(extension_frequency_index < arraysize(kSampleRates));
    extension_frequency_ = kSampleRates[extension_frequency_index];
  }

  RCHECK(channel_config_ < arraysize(kChannelConfigs));
  num_channels_ = kChannelConfigs[channel_config_];

  return frequency_ != 0 && num_channels_ != 0 && audio_object_type_ >= 1 &&
         audio_object_type_ <= 4 && frequency_index_ != 0xf &&
         channel_config_ <= 7;
}

bool AACAudioSpecificConfig::ConvertToADTS(std::vector<uint8_t>* buffer) const {
  size_t size = buffer->size() + kADTSHeaderSize;

  DCHECK(audio_object_type_ >= 1 && audio_object_type_ <= 4 &&
         frequency_index_ != 0xf && channel_config_ <= 7);

  // ADTS header uses 13 bits for packet size.
  if (size >= (1 << 13))
    return false;

  std::vector<uint8_t>& adts = *buffer;

  adts.insert(buffer->begin(), kADTSHeaderSize, 0);
  adts[0] = 0xff;
  adts[1] = 0xf1;
  adts[2] = ((audio_object_type_ - 1) << 6) + (frequency_index_ << 2) +
            (channel_config_ >> 2);
  adts[3] = ((channel_config_ & 0x3) << 6) + static_cast<uint8_t>(size >> 11);
  adts[4] = static_cast<uint8_t>((size & 0x7ff) >> 3);
  adts[5] = static_cast<uint8_t>(((size & 7) << 5) + 0x1f);
  adts[6] = 0xfc;

  return true;
}

AACAudioSpecificConfig::AudioObjectType
AACAudioSpecificConfig::GetAudioObjectType() const {
  if (ps_present_)
    return AOT_PS;
  if (sbr_present_)
    return AOT_SBR;
  return audio_object_type_;
}

uint32_t AACAudioSpecificConfig::GetSamplesPerSecond() const {
  if (extension_frequency_ > 0)
    return extension_frequency_;

  if (!sbr_present_)
    return frequency_;

  // The following code is written according to ISO 14496 Part 3 Table 1.11 and
  // Table 1.22. (Table 1.11 refers to the capping to 48000, Table 1.22 refers
  // to SBR doubling the AAC sample rate.)
  DCHECK_GT(frequency_, 0u);
  return std::min(2 * frequency_, 48000u);
}

uint8_t AACAudioSpecificConfig::GetNumChannels() const {
  // Check for implicit signalling of HE-AAC and indicate stereo output
  // if the mono channel configuration is signalled.
  // See ISO-14496-3 Section 1.6.6.1.2 for details about this special casing.
  if (sbr_present_ && channel_config_ == 1)
    return 2;  // CHANNEL_LAYOUT_STEREO

  // When Parametric Stereo is on, mono will be played as stereo.
  if (ps_present_ && channel_config_ == 1)
    return 2;  // CHANNEL_LAYOUT_STEREO

  return num_channels_;
}

// Currently this function only support GASpecificConfig defined in
// ISO 14496 Part 3 Table 4.1 - Syntax of GASpecificConfig()
bool AACAudioSpecificConfig::SkipDecoderGASpecificConfig(BitReader* bit_reader) const {
  switch (audio_object_type_) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
    case 7:
    case 17:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
      return SkipGASpecificConfig(bit_reader);
    default:
      break;
  }

  return false;
}

bool AACAudioSpecificConfig::SkipErrorSpecificConfig() const {
  switch (audio_object_type_) {
    case 17:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
      return false;
    default:
      break;
  }

  return true;
}

// The following code is written according to ISO 14496 part 3 Table 4.1 -
// GASpecificConfig.
bool AACAudioSpecificConfig::SkipGASpecificConfig(BitReader* bit_reader) const {
  uint8_t extension_flag = 0;
  uint8_t depends_on_core_coder;
  uint16_t dummy;

  RCHECK(bit_reader->ReadBits(1, &dummy));  // frameLengthFlag
  RCHECK(bit_reader->ReadBits(1, &depends_on_core_coder));
  if (depends_on_core_coder == 1)
    RCHECK(bit_reader->ReadBits(14, &dummy));  // coreCoderDelay

  RCHECK(bit_reader->ReadBits(1, &extension_flag));
  RCHECK(channel_config_ != 0);

  if (audio_object_type_ == 6 || audio_object_type_ == 20)
    RCHECK(bit_reader->ReadBits(3, &dummy));  // layerNr

  if (extension_flag) {
    if (audio_object_type_ == 22) {
      RCHECK(bit_reader->ReadBits(5, &dummy));  // numOfSubFrame
      RCHECK(bit_reader->ReadBits(11, &dummy));  // layer_length
    }

    if (audio_object_type_ == 17 || audio_object_type_ == 19 ||
        audio_object_type_ == 20 || audio_object_type_ == 23) {
      RCHECK(bit_reader->ReadBits(3, &dummy));  // resilience flags
    }

    RCHECK(bit_reader->ReadBits(1, &dummy));  // extensionFlag3
  }

  return true;
}

}  // namespace media
}  // namespace shaka
