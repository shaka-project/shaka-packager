// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/codecs/aac_audio_specific_config.h"

#include <algorithm>

#include "packager/base/logging.h"
#include "packager/media/base/bit_reader.h"
#include "packager/media/base/rcheck.h"

namespace shaka {
namespace media {
namespace {

// Sampling Frequency Index table, from ISO 14496-3 Table 1.16
static const uint32_t kSampleRates[] = {96000, 88200, 64000, 48000, 44100,
                                        32000, 24000, 22050, 16000, 12000,
                                        11025, 8000,  7350};

// Channel Configuration table, from ISO 14496-3 Table 1.17
const uint8_t kChannelConfigs[] = {0, 1, 2, 3, 4, 5, 6, 8};

// ISO 14496-3 Table 4.2 – Syntax of program_config_element()
// program_config_element()
//     ...
//     element_is_cpe[i]; 1 bslbf
//     element_tag_select[i]; 4 uimsbf
bool CountChannels(uint8_t num_elements,
                   uint8_t* num_channels,
                   BitReader* bit_reader) {
  for (uint8_t i = 0; i < num_elements; ++i) {
    bool is_pair = false;
    RCHECK(bit_reader->ReadBits(1, &is_pair));
    *num_channels += is_pair ? 2 : 1;
    RCHECK(bit_reader->SkipBits(4));
  }
  return true;
}

}  // namespace

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
  // Audio Object Types specified in "ISO/IEC 14496-3:2019, Table 1.19"
  RCHECK(ParseAudioObjectType(&reader));

  RCHECK(reader.ReadBits(4, &frequency_index_));
  if (frequency_index_ == 0xf)
    RCHECK(reader.ReadBits(24, &frequency_));
  RCHECK(reader.ReadBits(4, &channel_config_));

  RCHECK(channel_config_ < arraysize(kChannelConfigs));
  num_channels_ = kChannelConfigs[channel_config_];

  // Read extension configuration.
  if (audio_object_type_ == AOT_SBR || audio_object_type_ == AOT_PS) {
    sbr_present_ = audio_object_type_ == AOT_SBR;
    ps_present_ = audio_object_type_ == AOT_PS;
    extension_type = AOT_SBR;
    RCHECK(reader.ReadBits(4, &extension_frequency_index));
    if (extension_frequency_index == 0xf)
      RCHECK(reader.ReadBits(24, &extension_frequency_));
    RCHECK(ParseAudioObjectType(&reader));
  }

  RCHECK(ParseDecoderGASpecificConfig(&reader));
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
  
  if (audio_object_type_ == AOT_USAC) {
    return frequency_ != 0 && num_channels_ != 0 && channel_config_ <= 7;
  } else {
    return frequency_ != 0 && num_channels_ != 0 && audio_object_type_ >= 1 &&
           audio_object_type_ <= 4 && frequency_index_ != 0xf &&
           channel_config_ <= 7;
  }
}

bool AACAudioSpecificConfig::ConvertToADTS(
    const uint8_t* data,
    size_t data_size,
    std::vector<uint8_t>* audio_frame) const {
  DCHECK(audio_object_type_ >= 1 && audio_object_type_ <= 4 &&
         frequency_index_ != 0xf && channel_config_ <= 7);

  size_t size = kADTSHeaderSize + data_size;

  // ADTS header uses 13 bits for packet size.
  if (size >= (1 << 13))
    return false;

  audio_frame->reserve(size);
  audio_frame->resize(kADTSHeaderSize);

  audio_frame->at(0) = 0xff;
  audio_frame->at(1) = 0xf1;
  audio_frame->at(2) = ((audio_object_type_ - 1) << 6) +
                       (frequency_index_ << 2) + (channel_config_ >> 2);
  audio_frame->at(3) =
      ((channel_config_ & 0x3) << 6) + static_cast<uint8_t>(size >> 11);
  audio_frame->at(4) = static_cast<uint8_t>((size & 0x7ff) >> 3);
  audio_frame->at(5) = static_cast<uint8_t>(((size & 7) << 5) + 0x1f);
  audio_frame->at(6) = 0xfc;

  audio_frame->insert(audio_frame->end(), data, data + data_size);

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

bool AACAudioSpecificConfig::ParseAudioObjectType(BitReader* bit_reader) {
  RCHECK(bit_reader->ReadBits(5, &audio_object_type_));
  
  if (audio_object_type_ == AOT_ESCAPE) {
    uint8_t audioObjectTypeExt;
    RCHECK(bit_reader->ReadBits(6, &audioObjectTypeExt));
    audio_object_type_ = static_cast<AudioObjectType>(32 + audioObjectTypeExt);
  }
  
  return true;
}

// Currently this function only support GASpecificConfig defined in
// ISO 14496 Part 3 Table 4.1 - Syntax of GASpecificConfig()
bool AACAudioSpecificConfig::ParseDecoderGASpecificConfig(
    BitReader* bit_reader) {
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
      return ParseGASpecificConfig(bit_reader);
    case 42:
      // Skip UsacConfig() parsing until required
      RCHECK(bit_reader->SkipBits(bit_reader->bits_available()));
      return true;
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
bool AACAudioSpecificConfig::ParseGASpecificConfig(BitReader* bit_reader) {
  uint8_t extension_flag = 0;
  uint8_t depends_on_core_coder;
  uint16_t dummy;

  RCHECK(bit_reader->ReadBits(1, &dummy));  // frameLengthFlag
  RCHECK(bit_reader->ReadBits(1, &depends_on_core_coder));
  if (depends_on_core_coder == 1)
    RCHECK(bit_reader->ReadBits(14, &dummy));  // coreCoderDelay

  RCHECK(bit_reader->ReadBits(1, &extension_flag));
  if (channel_config_ == 0)
    RCHECK(ParseProgramConfigElement(bit_reader));

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

// ISO 14496-3 Table 4.2 – Syntax of program_config_element()
// program_config_element()
// {
//   element_instance_tag; 4 uimsbf
//   object_type; 2 uimsbf
//   sampling_frequency_index; 4 uimsbf
//   num_front_channel_elements; 4 uimsbf
//   num_side_channel_elements; 4 uimsbf
//   num_back_channel_elements; 4 uimsbf
//   num_lfe_channel_elements; 2 uimsbf
//   num_assoc_data_elements; 3 uimsbf
//   num_valid_cc_elements; 4 uimsbf
//   mono_mixdown_present; 1 uimsbf
//   if (mono_mixdown_present == 1)
//     mono_mixdown_element_number; 4 uimsbf
//   stereo_mixdown_present; 1 uimsbf
//   if (stereo_mixdown_present == 1)
//     stereo_mixdown_element_number; 4 uimsbf
//   matrix_mixdown_idx_present; 1 uimsbf
//   if (matrix_mixdown_idx_present == 1) {
//     matrix_mixdown_idx ; 2 uimsbf
//     pseudo_surround_enable; 1 uimsbf
//   }
//   for (i = 0; i < num_front_channel_elements; i++) {
//     front_element_is_cpe[i]; 1 bslbf
//     front_element_tag_select[i]; 4 uimsbf
//   }
//   for (i = 0; i < num_side_channel_elements; i++) {
//     side_element_is_cpe[i]; 1 bslbf
//     side_element_tag_select[i]; 4 uimsbf
//   }
//   for (i = 0; i < num_back_channel_elements; i++) {
//     back_element_is_cpe[i]; 1 bslbf
//     back_element_tag_select[i]; 4 uimsbf
//   }
//   for (i = 0; i < num_lfe_channel_elements; i++)
//     lfe_element_tag_select[i]; 4 uimsbf
//   for ( i = 0; i < num_assoc_data_elements; i++)
//     assoc_data_element_tag_select[i]; 4 uimsbf
//   for (i = 0; i < num_valid_cc_elements; i++) {
//     cc_element_is_ind_sw[i]; 1 uimsbf
//     valid_cc_element_tag_select[i]; 4 uimsbf
//   }
//   byte_alignment(); Note 1
//   comment_field_bytes; 8 uimsbf
//   for (i = 0; i < comment_field_bytes; i++)
//     comment_field_data[i]; 8 uimsbf
// }
// Note 1: If called from within an AudioSpecificConfig(), this
// byte_alignment shall be relative to the start of the AudioSpecificConfig().
bool AACAudioSpecificConfig::ParseProgramConfigElement(BitReader* bit_reader) {
  // element_instance_tag (4), object_type (2), sampling_frequency_index (4).
  RCHECK(bit_reader->SkipBits(4 + 2 + 4));

  uint8_t num_front_channel_elements = 0;
  uint8_t num_side_channel_elements = 0;
  uint8_t num_back_channel_elements = 0;
  uint8_t num_lfe_channel_elements = 0;
  RCHECK(bit_reader->ReadBits(4, &num_front_channel_elements));
  RCHECK(bit_reader->ReadBits(4, &num_side_channel_elements));
  RCHECK(bit_reader->ReadBits(4, &num_back_channel_elements));
  RCHECK(bit_reader->ReadBits(2, &num_lfe_channel_elements));

  uint8_t num_assoc_data_elements = 0;
  RCHECK(bit_reader->ReadBits(3, &num_assoc_data_elements));
  uint8_t num_valid_cc_elements = 0;
  RCHECK(bit_reader->ReadBits(4, &num_valid_cc_elements));

  RCHECK(bit_reader->SkipBitsConditional(true, 4));  // mono_mixdown
  RCHECK(bit_reader->SkipBitsConditional(true, 4));  // stereo_mixdown
  RCHECK(bit_reader->SkipBitsConditional(true, 3));  // matrix_mixdown_idx

  num_channels_ = 0;
  RCHECK(CountChannels(num_front_channel_elements, &num_channels_, bit_reader));
  RCHECK(CountChannels(num_side_channel_elements, &num_channels_, bit_reader));
  RCHECK(CountChannels(num_back_channel_elements, &num_channels_, bit_reader));
  num_channels_ += num_lfe_channel_elements;

  RCHECK(bit_reader->SkipBits(4 * num_lfe_channel_elements));
  RCHECK(bit_reader->SkipBits(4 * num_assoc_data_elements));
  RCHECK(bit_reader->SkipBits(5 * num_valid_cc_elements));

  bit_reader->SkipToNextByte();

  uint8_t comment_field_bytes = 0;
  RCHECK(bit_reader->ReadBits(8, &comment_field_bytes));
  RCHECK(bit_reader->SkipBytes(comment_field_bytes));
  return true;
}

}  // namespace media
}  // namespace shaka
