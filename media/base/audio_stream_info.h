// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_AUDIO_STREAM_INFO_H_
#define MEDIA_BASE_AUDIO_STREAM_INFO_H_

#include <vector>

#include "media/base/stream_info.h"

namespace edash_packager {
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

/// Holds audio stream information.
class AudioStreamInfo : public StreamInfo {
 public:
  /// Construct an initialized audio stream info object.
  AudioStreamInfo(int track_id,
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
                  bool is_encrypted);

  /// @name StreamInfo implementation overrides.
  /// @{
  virtual bool IsValidConfig() const OVERRIDE;
  virtual std::string ToString() const OVERRIDE;
  /// @}

  AudioCodec codec() const { return codec_; }
  uint8 sample_bits() const { return sample_bits_; }
  uint8 sample_bytes() const { return sample_bits_ / 8; }
  uint8 num_channels() const { return num_channels_; }
  uint32 sampling_frequency() const { return sampling_frequency_; }
  uint32 bytes_per_frame() const {
    return static_cast<uint32>(num_channels_) * sample_bits_ / 8;
  }

  void set_sampling_frequency(const uint32 sampling_frequency) {
    sampling_frequency_ = sampling_frequency;
  }


  /// @param audio_object_type is only used by AAC Codec, ignored otherwise.
  /// @return The codec string.
  static std::string GetCodecString(AudioCodec codec, uint8 audio_object_type);

 private:
  virtual ~AudioStreamInfo();

  AudioCodec codec_;
  uint8 sample_bits_;
  uint8 num_channels_;
  uint32 sampling_frequency_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_AUDIO_STREAM_INFO_H_
