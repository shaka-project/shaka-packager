// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_VIDEO_STREAM_INFO_H_
#define MEDIA_BASE_VIDEO_STREAM_INFO_H_

#include "packager/media/base/stream_info.h"

namespace edash_packager {
namespace media {

enum VideoCodec {
  kUnknownVideoCodec = 0,
  kCodecH264,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
  kCodecVP9,
  kNumVideoCodec
};

/// Holds video stream information.
class VideoStreamInfo : public StreamInfo {
 public:
  /// Construct an initialized video stream info object.
  /// If @a codec is @a kCodecH264 and either @pixel_width and @pixel_height is
  /// 0 (unknown), then this tries to parse @extra_data to extract the pixel
  /// width and height from it.
  /// @param pixel_width is the width of the pixel. 0 if unknown.
  /// @param pixel_height is the height of the pixels. 0 if unknown.
  VideoStreamInfo(int track_id,
                  uint32_t time_scale,
                  uint64_t duration,
                  VideoCodec codec,
                  const std::string& codec_string,
                  const std::string& language,
                  uint16_t width,
                  uint16_t height,
                  uint32_t pixel_width,
                  uint32_t pixel_height,
                  int16_t trick_play_rate,
                  uint8_t nalu_length_size,
                  const uint8_t* extra_data,
                  size_t extra_data_size,
                  bool is_encrypted);

  /// @name StreamInfo implementation overrides.
  /// @{
  virtual bool IsValidConfig() const OVERRIDE;
  virtual std::string ToString() const OVERRIDE;
  /// @}

  VideoCodec codec() const { return codec_; }
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

  /// @param profile,compatible_profiles,level are only used by H.264 codec.
  /// @return The codec string.
  static std::string GetCodecString(VideoCodec codec,
                                    uint8_t profile,
                                    uint8_t compatible_profiles,
                                    uint8_t level);

 private:
  virtual ~VideoStreamInfo();

  VideoCodec codec_;
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

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_VIDEO_STREAM_INFO_H_
