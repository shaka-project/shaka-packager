// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_VIDEO_STREAM_INFO_H_
#define MEDIA_BASE_VIDEO_STREAM_INFO_H_

#include "media/base/stream_info.h"

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
  VideoStreamInfo(int track_id,
                  uint32 time_scale,
                  uint64 duration,
                  VideoCodec codec,
                  const std::string& codec_string,
                  const std::string& language,
                  uint16 width,
                  uint16 height,
                  uint8 nalu_length_size,
                  const uint8* extra_data,
                  size_t extra_data_size,
                  bool is_encrypted);

  /// @name StreamInfo implementation overrides.
  /// @{
  virtual bool IsValidConfig() const OVERRIDE;
  virtual std::string ToString() const OVERRIDE;
  /// @}

  VideoCodec codec() const { return codec_; }
  uint16 width() const { return width_; }
  uint16 height() const { return height_; }
  uint8 nalu_length_size() const { return nalu_length_size_; }

  /// @param profile,compatible_profiles,level are only used by H.264 codec.
  /// @return The codec string.
  static std::string GetCodecString(VideoCodec codec,
                                    uint8 profile,
                                    uint8 compatible_profiles,
                                    uint8 level);

 private:
  virtual ~VideoStreamInfo();

  VideoCodec codec_;
  uint16 width_;
  uint16 height_;

  // Specifies the normalized size of the NAL unit length field. Can be 1, 2 or
  // 4 bytes, or 0 if the size if unknown or the stream is not a AVC stream
  // (H.264).
  uint8 nalu_length_size_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_STREAM_INFO_H_
