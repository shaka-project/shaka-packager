// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

class VideoStreamInfo : public StreamInfo {
 public:
  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  VideoStreamInfo(int track_id,
                  int time_scale,
                  VideoCodec codec,
                  int width,
                  int height,
                  const uint8* extra_data,
                  size_t extra_data_size,
                  bool is_encrypted);

  virtual ~VideoStreamInfo();

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  virtual bool IsValidConfig() const;

  // Returns a human-readable string describing |*this|.
  virtual std::string ToString();

  VideoCodec codec() const { return codec_; }
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  VideoCodec codec_;
  int width_;
  int height_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_STREAM_INFO_H_
