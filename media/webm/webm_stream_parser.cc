// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_stream_parser.h"

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "media/webm/webm_cluster_parser.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_content_encodings.h"
#include "media/webm/webm_crypto_helpers.h"
#include "media/webm/webm_info_parser.h"
#include "media/webm/webm_tracks_parser.h"

namespace media {

WebMStreamParser::WebMStreamParser()
    : state_(kWaitingForInit),
      waiting_for_buffers_(false) {
}

WebMStreamParser::~WebMStreamParser() {
  STLDeleteValues(&text_track_map_);
}

void WebMStreamParser::Init(const InitCB& init_cb,
                            const NewConfigCB& config_cb,
                            const NewBuffersCB& new_buffers_cb,
                            const NewTextBuffersCB& text_cb,
                            const NeedKeyCB& need_key_cb,
                            const AddTextTrackCB& add_text_track_cb,
                            const NewMediaSegmentCB& new_segment_cb,
                            const base::Closure& end_of_segment_cb,
                            const LogCB& log_cb) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());
  DCHECK(!config_cb.is_null());
  DCHECK(!new_buffers_cb.is_null());
  DCHECK(!text_cb.is_null());
  DCHECK(!need_key_cb.is_null());
  DCHECK(!new_segment_cb.is_null());
  DCHECK(!end_of_segment_cb.is_null());

  ChangeState(kParsingHeaders);
  init_cb_ = init_cb;
  config_cb_ = config_cb;
  new_buffers_cb_ = new_buffers_cb;
  text_cb_ = text_cb;
  need_key_cb_ = need_key_cb;
  add_text_track_cb_ = add_text_track_cb;
  new_segment_cb_ = new_segment_cb;
  end_of_segment_cb_ = end_of_segment_cb;
  log_cb_ = log_cb;
}

void WebMStreamParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);

  byte_queue_.Reset();

  if (state_ != kParsingClusters)
    return;

  cluster_parser_->Reset();
}

bool WebMStreamParser::Parse(const uint8* buf, int size) {
  DCHECK_NE(state_, kWaitingForInit);

  if (state_ == kError)
    return false;

  byte_queue_.Push(buf, size);

  int result = 0;
  int bytes_parsed = 0;
  const uint8* cur = NULL;
  int cur_size = 0;

  byte_queue_.Peek(&cur, &cur_size);
  while (cur_size > 0) {
    State oldState = state_;
    switch (state_) {
      case kParsingHeaders:
        result = ParseInfoAndTracks(cur, cur_size);
        break;

      case kParsingClusters:
        result = ParseCluster(cur, cur_size);
        break;

      case kWaitingForInit:
      case kError:
        return false;
    }

    if (result < 0) {
      ChangeState(kError);
      return false;
    }

    if (state_ == oldState && result == 0)
      break;

    DCHECK_GE(result, 0);
    cur += result;
    cur_size -= result;
    bytes_parsed += result;
  }

  byte_queue_.Pop(bytes_parsed);
  return true;
}

void WebMStreamParser::ChangeState(State new_state) {
  DVLOG(1) << "ChangeState() : " << state_ << " -> " << new_state;
  state_ = new_state;
}

int WebMStreamParser::ParseInfoAndTracks(const uint8* data, int size) {
  DVLOG(2) << "ParseInfoAndTracks()";
  DCHECK(data);
  DCHECK_GT(size, 0);

  const uint8* cur = data;
  int cur_size = size;
  int bytes_parsed = 0;

  int id;
  int64 element_size;
  int result = WebMParseElementHeader(cur, cur_size, &id, &element_size);

  if (result <= 0)
    return result;

  switch (id) {
    case kWebMIdEBMLHeader:
    case kWebMIdSeekHead:
    case kWebMIdVoid:
    case kWebMIdCRC32:
    case kWebMIdCues:
    case kWebMIdChapters:
      if (cur_size < (result + element_size)) {
        // We don't have the whole element yet. Signal we need more data.
        return 0;
      }
      // Skip the element.
      return result + element_size;
      break;
    case kWebMIdSegment:
      // Just consume the segment header.
      return result;
      break;
    case kWebMIdInfo:
      // We've found the element we are looking for.
      break;
    default: {
      MEDIA_LOG(log_cb_) << "Unexpected element ID 0x" << std::hex << id;
      return -1;
    }
  }

  WebMInfoParser info_parser;
  result = info_parser.Parse(cur, cur_size);

  if (result <= 0)
    return result;

  cur += result;
  cur_size -= result;
  bytes_parsed += result;

  WebMTracksParser tracks_parser(log_cb_, add_text_track_cb_.is_null());
  result = tracks_parser.Parse(cur, cur_size);

  if (result <= 0)
    return result;

  bytes_parsed += result;

  base::TimeDelta duration = kInfiniteDuration();

  if (info_parser.duration() > 0) {
    double mult = info_parser.timecode_scale() / 1000.0;
    int64 duration_in_us = info_parser.duration() * mult;
    duration = base::TimeDelta::FromMicroseconds(duration_in_us);
  }

  const AudioDecoderConfig& audio_config = tracks_parser.audio_decoder_config();
  if (audio_config.is_encrypted())
    FireNeedKey(tracks_parser.audio_encryption_key_id());

  const VideoDecoderConfig& video_config = tracks_parser.video_decoder_config();
  if (video_config.is_encrypted())
    FireNeedKey(tracks_parser.video_encryption_key_id());

  if (!config_cb_.Run(audio_config, video_config)) {
    DVLOG(1) << "New config data isn't allowed.";
    return -1;
  }

  typedef WebMTracksParser::TextTracks TextTracks;
  const TextTracks& text_tracks = tracks_parser.text_tracks();

  for (TextTracks::const_iterator itr = text_tracks.begin();
       itr != text_tracks.end(); ++itr) {
    const WebMTracksParser::TextTrackInfo& text_track_info = itr->second;

    // TODO(matthewjheaney): verify that WebVTT uses ISO 639-2 for lang
    scoped_ptr<TextTrack> text_track =
        add_text_track_cb_.Run(text_track_info.kind,
                               text_track_info.name,
                               text_track_info.language);

    // Assume ownership of pointer, and cache the text track object, for use
    // later when we have text track buffers. (The text track objects are
    // deallocated in the dtor for this class.)

    if (text_track)
      text_track_map_.insert(std::make_pair(itr->first, text_track.release()));
  }

  cluster_parser_.reset(new WebMClusterParser(
      info_parser.timecode_scale(),
      tracks_parser.audio_track_num(),
      tracks_parser.video_track_num(),
      text_tracks,
      tracks_parser.ignored_tracks(),
      tracks_parser.audio_encryption_key_id(),
      tracks_parser.video_encryption_key_id(),
      log_cb_));

  ChangeState(kParsingClusters);

  if (!init_cb_.is_null()) {
    init_cb_.Run(true, duration);
    init_cb_.Reset();
  }

  return bytes_parsed;
}

int WebMStreamParser::ParseCluster(const uint8* data, int size) {
  if (!cluster_parser_)
    return -1;

  int id;
  int64 element_size;
  int result = WebMParseElementHeader(data, size, &id, &element_size);

  if (result <= 0)
    return result;

  if (id == kWebMIdCluster)
    waiting_for_buffers_ = true;

  // TODO(matthewjheaney): implement support for chapters
  if (id == kWebMIdCues || id == kWebMIdChapters) {
    if (size < (result + element_size)) {
      // We don't have the whole element yet. Signal we need more data.
      return 0;
    }
    // Skip the element.
    return result + element_size;
  }

  if (id == kWebMIdEBMLHeader) {
    ChangeState(kParsingHeaders);
    return 0;
  }

  int bytes_parsed = cluster_parser_->Parse(data, size);

  if (bytes_parsed <= 0)
    return bytes_parsed;

  const BufferQueue& audio_buffers = cluster_parser_->audio_buffers();
  const BufferQueue& video_buffers = cluster_parser_->video_buffers();
  bool cluster_ended = cluster_parser_->cluster_ended();

  if (waiting_for_buffers_ &&
      cluster_parser_->cluster_start_time() != kNoTimestamp()) {
    new_segment_cb_.Run();
    waiting_for_buffers_ = false;
  }

  if ((!audio_buffers.empty() || !video_buffers.empty()) &&
      !new_buffers_cb_.Run(audio_buffers, video_buffers)) {
    return -1;
  }

  WebMClusterParser::TextTrackIterator text_track_iter =
    cluster_parser_->CreateTextTrackIterator();

  int text_track_num;
  const BufferQueue* text_buffers;

  while (text_track_iter(&text_track_num, &text_buffers)) {
    TextTrackMap::iterator find_result = text_track_map_.find(text_track_num);

    if (find_result == text_track_map_.end())
      continue;

    TextTrack* const text_track = find_result->second;

    if (!text_buffers->empty() && !text_cb_.Run(text_track, *text_buffers))
      return -1;
  }

  if (cluster_ended)
    end_of_segment_cb_.Run();

  return bytes_parsed;
}

void WebMStreamParser::FireNeedKey(const std::string& key_id) {
  int key_id_size = key_id.size();
  DCHECK_GT(key_id_size, 0);
  scoped_ptr<uint8[]> key_id_array(new uint8[key_id_size]);
  memcpy(key_id_array.get(), key_id.data(), key_id_size);
  need_key_cb_.Run(kWebMEncryptInitDataType, key_id_array.Pass(), key_id_size);
}

}  // namespace media
