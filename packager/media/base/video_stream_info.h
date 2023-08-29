// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_VIDEO_STREAM_INFO_H_
#define PACKAGER_MEDIA_BASE_VIDEO_STREAM_INFO_H_

#include "packager/media/base/stream_info.h"

namespace shaka {
namespace media {

enum class H26xStreamFormat {
  kUnSpecified,
  kAnnexbByteStream,
  kNalUnitStreamWithParameterSetNalus,
  kNalUnitStreamWithoutParameterSetNalus,
};

/// Holds video stream information.
class VideoStreamInfo : public StreamInfo {
 public:
  VideoStreamInfo() = default;

  /// Construct an initialized video stream info object.
  /// @param pixel_width is the width of the pixel. 0 if unknown.
  /// @param pixel_height is the height of the pixels. 0 if unknown.
  VideoStreamInfo(int track_id,
                  int32_t time_scale,
                  int64_t duration,
                  Codec codec,
                  H26xStreamFormat h26x_stream_format,
                  const std::string& codec_string,
                  const uint8_t* codec_config,
                  size_t codec_config_size,
                  uint16_t width,
                  uint16_t height,
                  uint32_t pixel_width,
                  uint32_t pixel_height,
                  uint8_t transfer_characteristics,
                  uint32_t trick_play_factor,
                  uint8_t nalu_length_size,
                  const std::string& language,
                  bool is_encrypted);

  ~VideoStreamInfo() override;

  /// @name StreamInfo implementation overrides.
  /// @{
  bool IsValidConfig() const override;
  std::string ToString() const override;
  std::unique_ptr<StreamInfo> Clone() const override;
  /// @}

  const std::vector<uint8_t>& extra_config() const { return extra_config_; }
  H26xStreamFormat h26x_stream_format() const { return h26x_stream_format_; }
  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  /// Returns the pixel width.
  /// @return 0 if unknown.
  uint32_t pixel_width() const { return pixel_width_; }
  /// Returns the pixel height.
  /// @return 0 if unknown.
  uint32_t pixel_height() const { return pixel_height_; }
  uint8_t transfer_characteristics() const { return transfer_characteristics_; }
  uint8_t nalu_length_size() const { return nalu_length_size_; }
  uint32_t trick_play_factor() const { return trick_play_factor_; }
  uint32_t playback_rate() const { return playback_rate_; }
  const std::vector<uint8_t>& eme_init_data() const { return eme_init_data_; }
  const std::vector<uint8_t>& colr_data() const { return colr_data_; }

  void set_extra_config(const std::vector<uint8_t>& extra_config) {
    extra_config_ = extra_config;
  }
  void set_width(uint32_t width) { width_ = width; }
  void set_height(uint32_t height) { height_ = height; }
  void set_pixel_width(uint32_t pixel_width) { pixel_width_ = pixel_width; }
  void set_pixel_height(uint32_t pixel_height) { pixel_height_ = pixel_height; }
  void set_transfer_characteristics(uint8_t transfer_characteristics) {
    transfer_characteristics_ = transfer_characteristics;
  }
  void set_trick_play_factor(uint32_t trick_play_factor) {
    trick_play_factor_ = trick_play_factor;
  }
  void set_playback_rate(uint32_t playback_rate) {
    playback_rate_ = playback_rate;
  }
  void set_eme_init_data(const uint8_t* eme_init_data,
                         size_t eme_init_data_size) {
    eme_init_data_.assign(eme_init_data, eme_init_data + eme_init_data_size);
  }
  void set_colr_data(const uint8_t* colr_data, size_t colr_data_size) {
    colr_data_.assign(colr_data, colr_data + colr_data_size);
  }

 private:
  // Extra codec configuration in a stream of mp4 boxes. It is only applicable
  // to mp4 container only. It is needed by some codecs, e.g. Dolby Vision.
  std::vector<uint8_t> extra_config_;
  H26xStreamFormat h26x_stream_format_;
  uint16_t width_;
  uint16_t height_;

  // pixel_width_:pixel_height_ is the sample aspect ratio.
  // 0 means unknown.
  uint32_t pixel_width_;
  uint32_t pixel_height_;
  uint8_t transfer_characteristics_ = 0;
  uint32_t trick_play_factor_ = 0;  // Non-zero for trick-play streams.

  // Playback rate is the attribute for trick play stream, which signals the
  // playout capabilities
  // (http://dashif.org/wp-content/uploads/2016/12/DASH-IF-IOP-v4.0-clean.pdf,
  // page 18, line 1). It is the ratio of main frame rate to the trick play
  // frame rate. If the time scale and frame duration are not modified after
  // trick play handler processing, the playback_rate equals to the number of
  // frames between consecutive key frames selected for trick play stream. For
  // example, if the video stream has GOP size of 10 and the trick play factor
  // is 3, the key frames are in this trick play stream are [frame_0, frame_30,
  // frame_60, ...]. Then the playback_rate is 30.
  // Non-zero for trick-play streams.
  uint32_t playback_rate_ = 0;

  // Specifies the size of the NAL unit length field. Can be 1, 2 or 4 bytes, or
  // 0 if the stream is not a NAL structured video stream or if it is an AnnexB
  // byte stream.
  uint8_t nalu_length_size_;

  // Container-specific data used by CDM to generate a license request:
  // https://w3c.github.io/encrypted-media/#initialization-data.
  std::vector<uint8_t> eme_init_data_;

  // Raw colr atom data. It is only applicable to the mp4 container.
  std::vector<uint8_t> colr_data_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_VIDEO_STREAM_INFO_H_
