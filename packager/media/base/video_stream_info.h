// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_VIDEO_STREAM_INFO_H_
#define MEDIA_BASE_VIDEO_STREAM_INFO_H_

#include "packager/media/base/stream_info.h"

namespace shaka {
namespace media {

/// Holds video stream information.
class VideoStreamInfo : public StreamInfo {
 public:
  /// Construct an initialized video stream info object.
  /// @param pixel_width is the width of the pixel. 0 if unknown.
  /// @param pixel_height is the height of the pixels. 0 if unknown.
  VideoStreamInfo(int track_id, uint32_t time_scale, uint64_t duration,
                  Codec codec, const std::string& codec_string,
                  const uint8_t* codec_config, size_t codec_config_size,
                  uint16_t width, uint16_t height, uint32_t pixel_width,
                  uint32_t pixel_height, int16_t trick_play_rate,
                  uint8_t nalu_length_size, const std::string& language,
                  bool is_encrypted);

  /// @name StreamInfo implementation overrides.
  /// @{
  bool IsValidConfig() const override;
  std::string ToString() const override;
  /// @}

  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  /// Returns the pixel width.
  /// @return 0 if unknown.
  uint32_t pixel_width() const { return pixel_width_; }
  /// Returns the pixel height.
  /// @return 0 if unknown.
  uint32_t pixel_height() const { return pixel_height_; }
  uint8_t nalu_length_size() const { return nalu_length_size_; }
  int16_t trick_play_rate() const { return trick_play_rate_; }
  const std::vector<uint8_t>& eme_init_data() const { return eme_init_data_; }

  void set_width(uint32_t width) { width_ = width; }
  void set_height(uint32_t height) { height_ = height; }
  void set_pixel_width(uint32_t pixel_width) { pixel_width_ = pixel_width; }
  void set_pixel_height(uint32_t pixel_height) { pixel_height_ = pixel_height; }
  void set_eme_init_data(const uint8_t* eme_init_data,
                         size_t eme_init_data_size) {
    eme_init_data_.assign(eme_init_data, eme_init_data + eme_init_data_size);
  }

 private:
  ~VideoStreamInfo() override;

  uint16_t width_;
  uint16_t height_;

  // pixel_width_:pixel_height_ is the sample aspect ratio.
  // 0 means unknown.
  uint32_t pixel_width_;
  uint32_t pixel_height_;
  int16_t trick_play_rate_;  // Non-zero for trick-play streams.

  // Specifies the normalized size of the NAL unit length field. Can be 1, 2 or
  // 4 bytes, or 0 if the size if unknown or the stream is not a AVC stream
  // (H.264).
  uint8_t nalu_length_size_;

  // Container-specific data used by CDM to generate a license request:
  // https://w3c.github.io/encrypted-media/#initialization-data.
  std::vector<uint8_t> eme_init_data_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_VIDEO_STREAM_INFO_H_
