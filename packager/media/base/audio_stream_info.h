// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_AUDIO_STREAM_INFO_H_
#define MEDIA_BASE_AUDIO_STREAM_INFO_H_

#include <vector>

#include "packager/media/base/stream_info.h"

namespace edash_packager {
namespace media {

enum AudioCodec {
  kUnknownAudioCodec = 0,
  kCodecAAC,
  kCodecAC3,
  kCodecDTSC,
  kCodecDTSE,
  kCodecDTSH,
  kCodecDTSL,
  kCodecDTSM,
  kCodecDTSP,
  kCodecOpus,
  kCodecVorbis,

  kNumAudioCodec
};

/// Holds audio stream information.
class AudioStreamInfo : public StreamInfo {
 public:
  /// Construct an initialized audio stream info object.
  AudioStreamInfo(int track_id,
                  uint32_t time_scale,
                  uint64_t duration,
                  AudioCodec codec,
                  const std::string& codec_string,
                  const std::string& language,
                  uint8_t sample_bits,
                  uint8_t num_channels,
                  uint32_t sampling_frequency,
                  uint32_t max_bitrate,
                  uint32_t avg_bitrate,
                  const uint8_t* extra_data,
                  size_t extra_data_size,
                  bool is_encrypted);

  /// @name StreamInfo implementation overrides.
  /// @{
  bool IsValidConfig() const override;
  std::string ToString() const override;
  /// @}

  AudioCodec codec() const { return codec_; }
  uint8_t sample_bits() const { return sample_bits_; }
  uint8_t sample_bytes() const { return sample_bits_ / 8; }
  uint8_t num_channels() const { return num_channels_; }
  uint32_t sampling_frequency() const { return sampling_frequency_; }
  uint32_t bytes_per_frame() const {
    return static_cast<uint32_t>(num_channels_) * sample_bits_ / 8;
  }
  uint32_t max_bitrate() const { return max_bitrate_; }
  uint32_t avg_bitrate() const { return avg_bitrate_; }

  void set_codec(AudioCodec codec) { codec_ = codec; }
  void set_sampling_frequency(const uint32_t sampling_frequency) {
    sampling_frequency_ = sampling_frequency;
  }

  /// @param audio_object_type is only used by AAC Codec, ignored otherwise.
  /// @return The codec string.
  static std::string GetCodecString(AudioCodec codec,
                                    uint8_t audio_object_type);

 private:
  ~AudioStreamInfo() override;

  AudioCodec codec_;
  uint8_t sample_bits_;
  uint8_t num_channels_;
  uint32_t sampling_frequency_;
  uint32_t max_bitrate_;
  uint32_t avg_bitrate_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_AUDIO_STREAM_INFO_H_
