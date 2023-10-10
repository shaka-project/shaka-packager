// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/codecs/hls_audio_util.h>

#include <absl/log/check.h>

#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/codecs/aac_audio_specific_config.h>

namespace shaka {
namespace media {

bool WriteAudioSetupInformation(Codec codec,
                                const uint8_t* audio_specific_config,
                                size_t audio_specific_config_size,
                                BufferWriter* audio_setup_information) {
  uint32_t audio_type = FOURCC_NULL;
  switch (codec) {
    case kCodecAAC: {
      AACAudioSpecificConfig config;
      const bool result = config.Parse(std::vector<uint8_t>(
          audio_specific_config,
          audio_specific_config + audio_specific_config_size));

      AACAudioSpecificConfig::AudioObjectType audio_object_type;
      if (!result) {
        LOG(WARNING) << "Failed to parse config. Assuming AAC-LC.";
        audio_object_type = AACAudioSpecificConfig::AOT_AAC_LC;
      } else {
        audio_object_type = config.GetAudioObjectType();
      }

      switch (audio_object_type) {
        case AACAudioSpecificConfig::AOT_AAC_LC:
          audio_type = FOURCC_zaac;
          break;
        case AACAudioSpecificConfig::AOT_SBR:
          audio_type = FOURCC_zach;
          break;
        case AACAudioSpecificConfig::AOT_PS:
          audio_type = FOURCC_zacp;
          break;
        default:
          LOG(ERROR) << "Unknown object type for aac " << audio_object_type;
          return false;
      }
    } break;
    case kCodecAC3:
      audio_type = FOURCC_zac3;
      break;
    case kCodecEAC3:
      audio_type = FOURCC_zec3;
      break;
    default:
      LOG(ERROR) << "Codec " << codec << " is not supported in encrypted TS.";
      return false;
  }

  DCHECK_NE(audio_type, FOURCC_NULL);
  audio_setup_information->AppendInt(audio_type);
  // Priming. Since no info from encoder, set it to 0x0000.
  audio_setup_information->AppendInt(static_cast<uint16_t>(0x0000));
  // Version is always 0x01.
  audio_setup_information->AppendInt(static_cast<uint8_t>(0x01));

  // Size is one byte.
  if (audio_specific_config_size > 0xFF) {
    LOG(ERROR) << "Audio specific config should not be larger than one byte "
               << audio_specific_config_size;
    return false;
  }
  audio_setup_information->AppendInt(
      static_cast<uint8_t>(audio_specific_config_size));

  audio_setup_information->AppendArray(audio_specific_config,
                                       audio_specific_config_size);
  return true;
}

}  // namespace media
}  // namespace shaka
