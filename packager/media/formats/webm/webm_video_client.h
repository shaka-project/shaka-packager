// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_VIDEO_CLIENT_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_VIDEO_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/codecs/vp_codec_configuration_record.h>
#include <packager/media/formats/webm/webm_parser.h>

namespace shaka {
namespace media {
class VideoDecoderConfig;

/// Helper class used to parse a Video element inside a TrackEntry element.
class WebMVideoClient : public WebMParserClient {
 public:
  WebMVideoClient();
  ~WebMVideoClient() override;

  /// Reset this object's state so it can process a new video track element.
  void Reset();

  /// Create a VideoStreamInfo with the data in |track_num|, |codec_id|,
  /// |codec_private|, |is_encrypted| and the fields parsed from the last video
  /// track element this object was used to parse.
  /// @return A VideoStreamInfo if successful.
  /// @return An empty pointer if there was unexpected values in the
  ///         provided parameters or video track element fields.
  std::shared_ptr<VideoStreamInfo> GetVideoStreamInfo(
      int64_t track_num,
      const std::string& codec_id,
      const std::vector<uint8_t>& codec_private,
      bool is_encrypted);

  /// Extracts VPCodecConfigurationRecord parsed from codec private data and
  /// Colour element.
  VPCodecConfigurationRecord GetVpCodecConfig(
      const std::vector<uint8_t>& codec_private);

 private:
  // WebMParserClient implementation.
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;
  bool OnFloat(int id, double val) override;

  int64_t pixel_width_ = -1;
  int64_t pixel_height_ = -1;
  int64_t crop_bottom_ = -1;
  int64_t crop_top_ = -1;
  int64_t crop_left_ = -1;
  int64_t crop_right_ = -1;
  int64_t display_width_ = -1;
  int64_t display_height_ = -1;
  int64_t display_unit_ = -1;
  int64_t alpha_mode_ = -1;

  int64_t matrix_coefficients_ = -1;
  int64_t bits_per_channel_ = -1;
  int64_t chroma_subsampling_horz_ = -1;
  int64_t chroma_subsampling_vert_ = -1;
  int64_t chroma_siting_horz_ = -1;
  int64_t chroma_siting_vert_ = -1;
  int64_t color_range_ = -1;
  int64_t transfer_characteristics_ = -1;
  int64_t color_primaries_ = -1;

  DISALLOW_COPY_AND_ASSIGN(WebMVideoClient);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_VIDEO_CLIENT_H_
