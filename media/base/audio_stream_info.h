// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_STREAM_INFO_H_
#define MEDIA_BASE_AUDIO_STREAM_INFO_H_

#include <vector>

#include "media/base/stream_info.h"

namespace media {

enum AudioCodec {
  kUnknownAudioCodec = 0,
  kCodecAAC,
  kCodecMP3,
  kCodecPCM,
  kCodecVorbis,
  kCodecFLAC,
  kCodecAMR_NB,
  kCodecAMR_WB,
  kCodecPCM_MULAW,
  kCodecGSM_MS,
  kCodecPCM_S16BE,
  kCodecPCM_S24BE,
  kCodecOpus,
  kCodecEAC3,

  kNumAudioCodec
};

class AudioStreamInfo : public StreamInfo {
 public:
  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  AudioStreamInfo(int track_id,
                  int time_scale,
                  AudioCodec codec,
                  int bytes_per_channel,
                  int num_channels,
                  int samples_per_second,
                  const uint8* extra_data,
                  size_t extra_data_size,
                  bool is_encrypted);

  virtual ~AudioStreamInfo();

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  virtual bool IsValidConfig() const;

  // Returns a human-readable string describing |*this|.
  virtual std::string ToString();

  AudioCodec codec() const { return codec_; }
  int bits_per_channel() const { return bytes_per_channel_ * 8; }
  int bytes_per_channel() const { return bytes_per_channel_; }
  int num_channels() const { return num_channels_; }
  int samples_per_second() const { return samples_per_second_; }
  int bytes_per_frame() const { return num_channels_ * bytes_per_channel_; }

 private:
  AudioCodec codec_;
  int bytes_per_channel_;
  int num_channels_;
  int samples_per_second_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_STREAM_INFO_H_
