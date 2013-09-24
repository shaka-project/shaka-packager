// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_STREAM_PARSER_H_
#define MEDIA_WEBM_WEBM_STREAM_PARSER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffers.h"
#include "media/base/byte_queue.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"

namespace media {

class WebMClusterParser;

class WebMStreamParser : public StreamParser {
 public:
  WebMStreamParser();
  virtual ~WebMStreamParser();

  // StreamParser implementation.
  virtual void Init(const InitCB& init_cb, const NewConfigCB& config_cb,
                    const NewBuffersCB& new_buffers_cb,
                    const NewTextBuffersCB& text_cb,
                    const NeedKeyCB& need_key_cb,
                    const AddTextTrackCB& add_text_track_cb,
                    const NewMediaSegmentCB& new_segment_cb,
                    const base::Closure& end_of_segment_cb,
                    const LogCB& log_cb) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual bool Parse(const uint8* buf, int size) OVERRIDE;

 private:
  enum State {
    kWaitingForInit,
    kParsingHeaders,
    kParsingClusters,
    kError
  };

  void ChangeState(State new_state);

  // Parses WebM Header, Info, Tracks elements. It also skips other level 1
  // elements that are not used right now. Once the Info & Tracks elements have
  // been parsed, this method will transition the parser from PARSING_HEADERS to
  // PARSING_CLUSTERS.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseInfoAndTracks(const uint8* data, int size);

  // Incrementally parses WebM cluster elements. This method also skips
  // CUES elements if they are encountered since we currently don't use the
  // data in these elements.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseCluster(const uint8* data, int size);

  // Fire needkey event through the |need_key_cb_|.
  void FireNeedKey(const std::string& key_id);

  State state_;
  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  NewTextBuffersCB text_cb_;
  NeedKeyCB need_key_cb_;
  AddTextTrackCB add_text_track_cb_;

  typedef std::map<int, TextTrack* > TextTrackMap;
  TextTrackMap text_track_map_;

  NewMediaSegmentCB new_segment_cb_;
  base::Closure end_of_segment_cb_;
  LogCB log_cb_;

  // True if a new cluster id has been seen, but no audio or video buffers have
  // been parsed yet.
  bool waiting_for_buffers_;

  scoped_ptr<WebMClusterParser> cluster_parser_;
  ByteQueue byte_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebMStreamParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_STREAM_PARSER_H_
