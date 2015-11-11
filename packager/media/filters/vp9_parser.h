// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILTERS_VP9_PARSER_H_
#define MEDIA_FILTERS_VP9_PARSER_H_

#include <stdint.h>
#include <stdlib.h>

#include "packager/base/macros.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/filters/vp_codec_configuration.h"

namespace edash_packager {
namespace media {

struct VPxFrameInfo {
  size_t frame_size;
  size_t uncompressed_header_size;
  bool is_key_frame;
  uint32_t width;
  uint32_t height;
};

/// Class to parse a vp9 bit stream.
class VP9Parser {
 public:
  VP9Parser();
  ~VP9Parser();

  /// Parse @a data with size @a data_size.
  /// @param data_size Size of the sample in bytes. Note that it should be a
  ///        full sample.
  /// @param[out] vpx_frames points to the list of VPx frames for the current
  ///             sample on success. Cannot be NULL.
  /// @return true on success, false otherwise.
  bool Parse(const uint8_t* data,
             size_t data_size,
             std::vector<VPxFrameInfo>* vpx_frames);

  /// @return VPx codec configuration extracted. Note that it is only valid
  ///         after parsing a key frame or intra frame successfully.
  const VPCodecConfiguration& codec_config() { return codec_config_; }

 private:
  // Keep track of the current width and height. Note that they may change from
  // frame to frame.
  uint32_t width_;
  uint32_t height_;

  VPCodecConfiguration codec_config_;

  DISALLOW_COPY_AND_ASSIGN(VP9Parser);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILTERS_VP9_PARSER_H_
