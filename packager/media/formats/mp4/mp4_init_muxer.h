// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_MP4_INIT_MUXER_H
#define PACKAGER_MEDIA_FORMATS_MP4_MP4_INIT_MUXER_H

#include <optional>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/muxer.h>
#include <packager/media/formats/mp4/mp4_muxer.h>

namespace shaka {
namespace media {

class AudioStreamInfo;
class StreamInfo;
class TextStreamInfo;
class VideoStreamInfo;

namespace mp4 {

class Segmenter;

struct ProtectionSchemeInfo;
struct Track;

/// Implements MP4 Muxer for ISO-BMFF. Please refer to ISO/IEC 14496-12: ISO
/// base media file format for details.
class MP4InitMuxer : public MP4Muxer {
 public:
  /// Create a MP4Muxer object from MuxerOptions.
  explicit MP4InitMuxer(const MuxerOptions& options);
  ~MP4InitMuxer() override;

 private:
  // Muxer implementation overrides.
  Status Finalize() override;
  Status DelayInitializeMuxer();

  DISALLOW_COPY_AND_ASSIGN(MP4InitMuxer);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_MP4_INIT_MUXER_H
