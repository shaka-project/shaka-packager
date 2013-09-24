// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_TRACKS_PARSER_H_
#define MEDIA_WEBM_WEBM_TRACKS_PARSER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_log.h"
#include "media/base/text_track.h"
#include "media/base/video_decoder_config.h"
#include "media/webm/webm_audio_client.h"
#include "media/webm/webm_content_encodings_client.h"
#include "media/webm/webm_parser.h"
#include "media/webm/webm_video_client.h"

namespace media {

// Parser for WebM Tracks element.
class MEDIA_EXPORT WebMTracksParser : public WebMParserClient {
 public:
  explicit WebMTracksParser(const LogCB& log_cb, bool ignore_text_tracks);
  virtual ~WebMTracksParser();

  // Parses a WebM Tracks element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  int64 audio_track_num() const { return audio_track_num_; }
  int64 video_track_num() const { return video_track_num_; }
  const std::set<int64>& ignored_tracks() const { return ignored_tracks_; }

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

  struct TextTrackInfo {
    TextKind kind;
    std::string name;
    std::string language;
  };

  typedef std::map<int64, TextTrackInfo> TextTracks;

  const TextTracks& text_tracks() const {
    return text_tracks_;
  }

 private:
  // WebMParserClient implementation.
  virtual WebMParserClient* OnListStart(int id) OVERRIDE;
  virtual bool OnListEnd(int id) OVERRIDE;
  virtual bool OnUInt(int id, int64 val) OVERRIDE;
  virtual bool OnFloat(int id, double val) OVERRIDE;
  virtual bool OnBinary(int id, const uint8* data, int size) OVERRIDE;
  virtual bool OnString(int id, const std::string& str) OVERRIDE;

  int64 track_type_;
  int64 track_num_;
  std::string track_name_;
  std::string track_language_;
  std::string codec_id_;
  std::vector<uint8> codec_private_;
  scoped_ptr<WebMContentEncodingsClient> track_content_encodings_client_;

  int64 audio_track_num_;
  int64 video_track_num_;
  bool ignore_text_tracks_;
  TextTracks text_tracks_;
  std::set<int64> ignored_tracks_;
  std::string audio_encryption_key_id_;
  std::string video_encryption_key_id_;
  LogCB log_cb_;

  WebMAudioClient audio_client_;
  AudioDecoderConfig audio_decoder_config_;

  WebMVideoClient video_client_;
  VideoDecoderConfig video_decoder_config_;

  DISALLOW_COPY_AND_ASSIGN(WebMTracksParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_TRACKS_PARSER_H_
