// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_MP4_INIT_MUXER_H
#define PACKAGER_MEDIA_FORMATS_MP4_MP4_INIT_MUXER_H

#include <packager/macros/classes.h>
#include <packager/media/base/muxer.h>
#include <packager/media/formats/mp4/mp4_muxer.h>

namespace shaka {
namespace media {
namespace mp4 {

/// An MP4 Muxer implementation for ISO-BMFF for init segments only.
/// Please refer to ISO/IEC 14496-12: ISO base media file format for details.
class MP4InitMuxer : public MP4Muxer {
 public:
  /// Create a MP4InitMuxer object from MuxerOptions.
  explicit MP4InitMuxer(const MuxerOptions& options);
  ~MP4InitMuxer() override;

 private:
  // Init segment Muxer implementation overrides.
  Status Finalize() override;

  DISALLOW_COPY_AND_ASSIGN(MP4InitMuxer);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_MP4_INIT_MUXER_H
