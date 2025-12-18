// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_TEXT_STREAM_INFO_H_
#define PACKAGER_MEDIA_BASE_TEXT_STREAM_INFO_H_

#include <cstdint>
#include <map>
#include <string>

#include <packager/media/base/stream_info.h>
#include <packager/media/base/text_sample.h>

namespace shaka {
namespace media {

struct TextRegion {
  /// The width of the region; percent units are relative to the window.
  TextNumber width{100, TextUnitType::kPercent};
  /// The height of the region; percent units are relative to the window.
  TextNumber height{100, TextUnitType::kPercent};

  /// The x and y coordinates of the anchor point within the window.  Percent
  /// units are relative to the window.  In WebVTT this is called the
  /// "viewport region anchor".
  TextNumber window_anchor_x{0, TextUnitType::kPercent};
  TextNumber window_anchor_y{0, TextUnitType::kPercent};
  /// The x and y coordinates of the anchor point within the region.  Percent
  /// units are relative to the region size.  For example: if this is
  /// (100, 100), then the bottom right of the region should be placed at the
  /// window anchor point.
  /// See https://www.w3.org/TR/webvtt1/#regions.
  TextNumber region_anchor_x{0, TextUnitType::kPercent};
  TextNumber region_anchor_y{0, TextUnitType::kPercent};

  /// If true, cues are scrolled up when adding new cues; if false, cues are
  /// added above existing cues or replace existing ones.
  bool scroll = false;
};

/// Contains info about a sub-stream within a text stream.  Depending on the
/// format, some info may not be available.  This info doesn't affect output.
struct TextSubStreamInfo {
  std::string language;
};

class TextStreamInfo : public StreamInfo {
 public:
  /// No encryption supported.
  /// @param track_id is the track ID of this stream.
  /// @param time_scale is the time scale of this stream.
  /// @param duration is the duration of this stream.
  /// @param codec is the media codec.
  /// @param codec_string is the codec in string format.
  /// @param codec_config is configuration for this text stream. This could be
  ///        the metadata that applies to all the samples of this stream. This
  ///        may be empty.
  /// @param width of the text. This may be 0.
  /// @param height of the text. This may be 0.
  /// @param language is the language of this stream. This may be empty.
  TextStreamInfo(int track_id,
                 int32_t time_scale,
                 int64_t duration,
                 Codec codec,
                 const std::string& codec_string,
                 const std::string& codec_config,
                 uint16_t width,
                 uint16_t height,
                 const std::string& language);

  ~TextStreamInfo() override;

  bool IsValidConfig() const override;

  std::string ToString() const override;
  std::unique_ptr<StreamInfo> Clone() const override;

  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  const std::map<std::string, TextRegion>& regions() const { return regions_; }
  void AddRegion(const std::string& id, const TextRegion& region) {
    regions_[id] = region;
  }
  const std::string& css_styles() const { return css_styles_; }
  void set_css_styles(const std::string& styles) { css_styles_ = styles; }

  void AddSubStream(uint16_t index, TextSubStreamInfo info) {
    sub_streams_.emplace(index, std::move(info));
  }
  const std::map<uint16_t, TextSubStreamInfo>& sub_streams() const {
    return sub_streams_;
  }

 private:
  std::map<std::string, TextRegion> regions_;
  std::map<uint16_t, TextSubStreamInfo> sub_streams_;
  std::string css_styles_;
  uint16_t width_;
  uint16_t height_;

  // Allow copying. This is very light weight.
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TEXT_STREAM_INFO_H_
