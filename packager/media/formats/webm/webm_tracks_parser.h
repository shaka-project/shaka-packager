// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "packager/base/compiler_specific.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/base/time/time.h"
#include "packager/media/base/audio_decoder_config.h"
#include "packager/media/base/text_track_config.h"
#include "packager/media/base/video_decoder_config.h"
#include "packager/media/formats/webm/webm_audio_client.h"
#include "packager/media/formats/webm/webm_content_encodings_client.h"
#include "packager/media/formats/webm/webm_parser.h"
#include "packager/media/formats/webm/webm_video_client.h"

namespace edash_packager {
namespace media {

// Parser for WebM Tracks element.
class WebMTracksParser : public WebMParserClient {
 public:
  explicit WebMTracksParser(bool ignore_text_tracks);
  ~WebMTracksParser() override;

  // Parses a WebM Tracks element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  int64_t audio_track_num() const { return audio_track_num_; }
  int64_t video_track_num() const { return video_track_num_; }

  // If TrackEntry DefaultDuration field existed for the associated audio or
  // video track, returns that value converted from ns to base::TimeDelta with
  // precision not greater than |timecode_scale_in_us|. Defaults to
  // kNoTimestamp().
  base::TimeDelta GetAudioDefaultDuration(
      const double timecode_scale_in_us) const;
  base::TimeDelta GetVideoDefaultDuration(
      const double timecode_scale_in_us) const;

  const std::set<int64_t>& ignored_tracks() const { return ignored_tracks_; }

  const std::string& audio_encryption_key_id() const {
    return audio_encryption_key_id_;
  }

  const AudioDecoderConfig& audio_decoder_config() {
    return audio_decoder_config_;
  }

  const std::string& video_encryption_key_id() const {
    return video_encryption_key_id_;
  }

  const VideoDecoderConfig& video_decoder_config() {
    return video_decoder_config_;
  }

  typedef std::map<int, TextTrackConfig> TextTracks;

  const TextTracks& text_tracks() const {
    return text_tracks_;
  }

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
  scoped_ptr<WebMContentEncodingsClient> track_content_encodings_client_;

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
  AudioDecoderConfig audio_decoder_config_;

  WebMVideoClient video_client_;
  VideoDecoderConfig video_decoder_config_;

  DISALLOW_COPY_AND_ASSIGN(WebMTracksParser);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_WEBM_WEBM_TRACKS_PARSER_H_
