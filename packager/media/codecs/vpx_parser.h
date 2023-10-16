// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_VPX_PARSER_H_
#define PACKAGER_MEDIA_CODECS_VPX_PARSER_H_

#include <cstdint>
#include <cstdlib>

#include <packager/macros/classes.h>
#include <packager/media/codecs/vp_codec_configuration_record.h>

namespace shaka {
namespace media {

struct VPxFrameInfo {
  size_t frame_size;
  size_t uncompressed_header_size;
  bool is_keyframe;
  uint32_t width;
  uint32_t height;
};

class VPxParser {
 public:
  VPxParser() {}
  virtual ~VPxParser() {}

  /// Parse @a data with size @a data_size.
  /// @param data_size Size of the sample in bytes. Note that it should be a
  ///        full sample.
  /// @param[out] vpx_frames points to the list of VPx frames for the current
  ///             sample on success. Cannot be NULL.
  /// @return true on success, false otherwise.
  virtual bool Parse(const uint8_t* data,
                     size_t data_size,
                     std::vector<VPxFrameInfo>* vpx_frames) = 0;

  /// @return VPx codec configuration extracted. Note that it is only valid
  ///         after parsing a keyframe or intra frame successfully.
  const VPCodecConfigurationRecord& codec_config() const {
    return codec_config_;
  }

 protected:
  VPCodecConfigurationRecord* writable_codec_config() { return &codec_config_; }

 private:
  VPCodecConfigurationRecord codec_config_;

  DISALLOW_COPY_AND_ASSIGN(VPxParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_VPX_PARSER_H_
