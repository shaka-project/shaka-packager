// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/audio_stream_info.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/base/limits.h"

namespace edash_packager {
namespace media {

namespace {
std::string AudioCodecToString(AudioCodec audio_codec) {
  switch (audio_codec) {
    case kCodecAAC:
      return "AAC";
    case kCodecMP3:
      return "MP3";
    case kCodecPCM:
      return "PCM";
    case kCodecVorbis:
      return "Vorbis";
    case kCodecFLAC:
      return "FLAC";
    case kCodecAMR_NB:
      return "AMR_NB";
    case kCodecAMR_WB:
      return "AMR_WB";
    case kCodecPCM_MULAW:
      return "PCM_MULAW";
    case kCodecGSM_MS:
      return "GSM_MS";
    case kCodecPCM_S16BE:
      return "PCM_S16BE";
    case kCodecPCM_S24BE:
      return "PCM_S24BE";
    case kCodecOpus:
      return "Opus";
    case kCodecEAC3:
      return "EAC3";
    default:
      NOTIMPLEMENTED() << "Unknown Audio Codec: " << audio_codec;
      return "UnknownAudioCodec";
  }
}
}  // namespace

AudioStreamInfo::AudioStreamInfo(int track_id,
                                 uint32_t time_scale,
                                 uint64_t duration,
                                 AudioCodec codec,
                                 const std::string& codec_string,
                                 const std::string& language,
                                 uint8_t sample_bits,
                                 uint8_t num_channels,
                                 uint32_t sampling_frequency,
                                 const uint8_t* extra_data,
                                 size_t extra_data_size,
                                 bool is_encrypted)
    : StreamInfo(kStreamAudio,
                 track_id,
                 time_scale,
                 duration,
                 codec_string,
                 language,
                 extra_data,
                 extra_data_size,
                 is_encrypted),
      codec_(codec),
      sample_bits_(sample_bits),
      num_channels_(num_channels),
      sampling_frequency_(sampling_frequency) {
}

AudioStreamInfo::~AudioStreamInfo() {}

bool AudioStreamInfo::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec && num_channels_ != 0 &&
         num_channels_ <= limits::kMaxChannels && sample_bits_ > 0 &&
         sample_bits_ <= limits::kMaxBitsPerSample &&
         sampling_frequency_ > 0 &&
         sampling_frequency_ <= limits::kMaxSampleRate;
}

std::string AudioStreamInfo::ToString() const {
  return base::StringPrintf(
      "%s codec: %s\n sample_bits: %d\n num_channels: %d\n "
      "sampling_frequency: %d\n",
      StreamInfo::ToString().c_str(),
      AudioCodecToString(codec_).c_str(),
      sample_bits_,
      num_channels_,
      sampling_frequency_);
}

std::string AudioStreamInfo::GetCodecString(AudioCodec codec,
                                            uint8_t audio_object_type) {
  switch (codec) {
    case kCodecVorbis:
      return "vorbis";
    case kCodecOpus:
      return "opus";
    case kCodecAAC:
      return "mp4a.40." + base::UintToString(audio_object_type);
    default:
      NOTIMPLEMENTED() << "Codec: " << codec;
      return "unknown";
  }
}

}  // namespace media
}  // namespace edash_packager
