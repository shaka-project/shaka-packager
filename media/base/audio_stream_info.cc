// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_stream_info.h"

#include <sstream>

#include "media/base/limits.h"

namespace media {

AudioStreamInfo::AudioStreamInfo(int track_id,
                                 int time_scale,
                                 AudioCodec codec,
                                 int bytes_per_channel,
                                 int num_channels,
                                 int samples_per_second,
                                 const uint8* extra_data,
                                 size_t extra_data_size,
                                 bool is_encrypted)
    : StreamInfo(kStreamAudio,
                 track_id,
                 time_scale,
                 extra_data,
                 extra_data_size,
                 is_encrypted),
      codec_(codec),
      bytes_per_channel_(bytes_per_channel),
      num_channels_(num_channels),
      samples_per_second_(samples_per_second) {}

AudioStreamInfo::~AudioStreamInfo() {}

bool AudioStreamInfo::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec && num_channels_ != 0 &&
         num_channels_ <= limits::kMaxChannels && bytes_per_channel_ > 0 &&
         bytes_per_channel_ <= limits::kMaxBytesPerSample &&
         samples_per_second_ > 0 &&
         samples_per_second_ <= limits::kMaxSampleRate;
}

std::string AudioStreamInfo::ToString() {
  std::ostringstream s;
  s << "codec: " << codec_
    << " bytes_per_channel: " << bytes_per_channel_
    << " num_channels: " << num_channels_
    << " samples_per_second: " << samples_per_second_
    << " " << StreamInfo::ToString();
  return s.str();
}

}  // namespace media
