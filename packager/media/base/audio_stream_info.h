// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_AUDIO_STREAM_INFO_H_
#define PACKAGER_MEDIA_BASE_AUDIO_STREAM_INFO_H_

#include <cstdint>
#include <vector>

#include <packager/media/base/stream_info.h>

namespace shaka {
namespace media {

/// Holds audio stream information.
class AudioStreamInfo : public StreamInfo {
 public:
  /// Construct an initialized audio stream info object.
  AudioStreamInfo(int track_id,
                  int32_t time_scale,
                  int64_t duration,
                  Codec codec,
                  const std::string& codec_string,
                  const uint8_t* codec_config,
                  size_t codec_config_size,
                  uint8_t sample_bits,
                  uint8_t num_channels,
                  uint32_t sampling_frequency,
                  uint64_t seek_preroll_ns,
                  uint64_t codec_delay_ns,
                  uint32_t max_bitrate,
                  uint32_t avg_bitrate,
                  const std::string& language,
                  bool is_encrypted);

  ~AudioStreamInfo() override;

  /// @name StreamInfo implementation overrides.
  /// @{
  bool IsValidConfig() const override;
  std::string ToString() const override;
  std::unique_ptr<StreamInfo> Clone() const override;
  /// @}

  uint8_t sample_bits() const { return sample_bits_; }
  uint8_t sample_bytes() const { return sample_bits_ / 8; }
  uint8_t num_channels() const { return num_channels_; }
  uint32_t sampling_frequency() const { return sampling_frequency_; }
  uint32_t bytes_per_frame() const {
    return static_cast<uint32_t>(num_channels_) * sample_bits_ / 8;
  }
  uint64_t seek_preroll_ns() const { return seek_preroll_ns_; }
  uint64_t codec_delay_ns() const { return codec_delay_ns_; }
  uint32_t max_bitrate() const { return max_bitrate_; }
  uint32_t avg_bitrate() const { return avg_bitrate_; }

  void set_sampling_frequency(const uint32_t sampling_frequency) {
    sampling_frequency_ = sampling_frequency;
  }

  void set_max_bitrate(const uint32_t max_bitrate) {
    max_bitrate_ = max_bitrate;
  }

  /// @param audio_object_type is only used by AAC Codec, ignored otherwise.
  /// @return The codec string.
  static std::string GetCodecString(Codec codec, uint8_t audio_object_type);

 private:
  uint8_t sample_bits_;
  uint8_t num_channels_;
  uint32_t sampling_frequency_;
  uint64_t seek_preroll_ns_;
  uint64_t codec_delay_ns_;
  uint32_t max_bitrate_;
  uint32_t avg_bitrate_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_AUDIO_STREAM_INFO_H_
