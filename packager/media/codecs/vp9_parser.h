// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_VP9_PARSER_H_
#define PACKAGER_MEDIA_CODECS_VP9_PARSER_H_

#include <cstdint>
#include <cstdlib>

#include <packager/macros/classes.h>
#include <packager/media/codecs/vpx_parser.h>

namespace shaka {
namespace media {

/// Class to parse a vp9 bit stream.
class VP9Parser : public VPxParser {
 public:
  VP9Parser();
  ~VP9Parser() override;

  /// Parse @a data with size @a data_size.
  /// @param data_size Size of the sample in bytes. Note that it should be a
  ///        full sample.
  /// @param[out] vpx_frames points to the list of VPx frames for the current
  ///             sample on success. Cannot be NULL.
  /// @return true on success, false otherwise.
  bool Parse(const uint8_t* data,
             size_t data_size,
             std::vector<VPxFrameInfo>* vpx_frames) override;

  /// A convenient utility function to check whether the frame is a keyframe.
  /// Note that this function does not do a full parse of the frame header, so
  /// should be more efficient than Parse().
  /// @param data_size Size of the sample in bytes.
  /// @return true if it is, false if it is not or if there is parsing error.
  static bool IsKeyframe(const uint8_t* data, size_t data_size);

 private:
  // Keep track of the current width and height. Note that they may change from
  // frame to frame.
  uint32_t width_;
  uint32_t height_;

  DISALLOW_COPY_AND_ASSIGN(VP9Parser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_VP9_PARSER_H_
