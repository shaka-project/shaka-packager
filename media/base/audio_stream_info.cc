// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_stream_info.h"

#include <sstream>

#include "base/strings/string_number_conversions.h"
#include "media/base/limits.h"

namespace media {

AudioStreamInfo::AudioStreamInfo(int track_id,
                                 uint32 time_scale,
                                 uint64 duration,
                                 AudioCodec codec,
                                 const std::string& codec_string,
                                 const std::string& language,
                                 uint8 sample_bits,
                                 uint8 num_channels,
                                 uint32 sampling_frequency,
                                 const uint8* extra_data,
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
      sampling_frequency_(sampling_frequency) {}

AudioStreamInfo::~AudioStreamInfo() {}

bool AudioStreamInfo::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec && num_channels_ != 0 &&
         num_channels_ <= limits::kMaxChannels && sample_bits_ > 0 &&
         sample_bits_ <= limits::kMaxBitsPerSample &&
         sampling_frequency_ > 0 &&
         sampling_frequency_ <= limits::kMaxSampleRate;
}

std::string AudioStreamInfo::ToString() const {
  std::ostringstream s;
  s << "codec: " << codec_
    << " sample_bits: " << static_cast<int>(sample_bits_)
    << " num_channels: " << static_cast<int>(num_channels_)
    << " sampling_frequency: " << sampling_frequency_
    << " " << StreamInfo::ToString();
  return s.str();
}

std::string AudioStreamInfo::GetCodecString(AudioCodec codec,
                                            uint8 audio_object_type) {
  switch (codec) {
    case kCodecVorbis:
      return "vorbis";
    case kCodecOpus:
      return "opus";
    case kCodecAAC:
      return "mp4a.40." + base::UintToString(audio_object_type);
  }
  return "unknown";
}

}  // namespace media
