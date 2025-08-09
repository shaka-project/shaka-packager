// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/text_track_config.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/formats/webm/webm_audio_client.h>
#include <packager/media/formats/webm/webm_content_encodings_client.h>
#include <packager/media/formats/webm/webm_parser.h>
#include <packager/media/formats/webm/webm_video_client.h>

namespace shaka {
namespace media {

/// Parser for WebM Tracks element.
class WebMTracksParser : public WebMParserClient {
 public:
  explicit WebMTracksParser(bool ignore_text_tracks);
  ~WebMTracksParser() override;

  /// Parses a WebM Tracks element in |buf|.
  /// @return -1 if the parse fails.
  /// @return 0 if more data is needed.
  /// @return The number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  int64_t audio_track_num() const { return audio_track_num_; }
  int64_t video_track_num() const { return video_track_num_; }

  /// If TrackEntry DefaultDuration field existed for the associated audio or
  /// video track, returns that value converted from ns to base::TimeDelta with
  /// precision not greater than |timecode_scale_in_us|. Defaults to
  /// kNoTimestamp.
  int64_t GetAudioDefaultDuration(const double timecode_scale_in_us) const;
  int64_t GetVideoDefaultDuration(const double timecode_scale_in_us) const;

  const std::set<int64_t>& ignored_tracks() const { return ignored_tracks_; }

  const std::string& audio_encryption_key_id() const {
    return audio_encryption_key_id_;
  }

  std::shared_ptr<AudioStreamInfo> audio_stream_info() {
    return audio_stream_info_;
  }

  const std::string& video_encryption_key_id() const {
    return video_encryption_key_id_;
  }

  std::shared_ptr<VideoStreamInfo> video_stream_info() {
    return video_stream_info_;
  }

  typedef std::map<int, TextTrackConfig> TextTracks;

  const TextTracks& text_tracks() const {
    return text_tracks_;
  }

  const VPCodecConfigurationRecord& vp_config() const { return vp_config_; }

 private:
  // WebMParserClient implementation.
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnFloat(int id, double val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;
  bool OnString(int id, const std::string& str) override;

  int64_t track_type_;
  int64_t track_num_;
  std::string track_name_;
  std::string track_language_;
  std::string codec_id_;
  std::vector<uint8_t> codec_private_;
  int64_t seek_preroll_;
  int64_t codec_delay_;
  int64_t default_duration_;
  std::unique_ptr<WebMContentEncodingsClient> track_content_encodings_client_;

  int64_t audio_track_num_;
  int64_t audio_default_duration_;
  int64_t video_track_num_;
  int64_t video_default_duration_;
  bool ignore_text_tracks_;
  TextTracks text_tracks_;
  std::set<int64_t> ignored_tracks_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;

  WebMAudioClient audio_client_;
  std::shared_ptr<AudioStreamInfo> audio_stream_info_;

  WebMVideoClient video_client_;
  VPCodecConfigurationRecord vp_config_;
  std::shared_ptr<VideoStreamInfo> video_stream_info_;

  DISALLOW_COPY_AND_ASSIGN(WebMTracksParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_
