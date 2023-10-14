// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/webm/webm_media_parser.h>

#include <string>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/timestamp.h>
#include <packager/media/formats/webm/webm_cluster_parser.h>
#include <packager/media/formats/webm/webm_constants.h>
#include <packager/media/formats/webm/webm_content_encodings.h>
#include <packager/media/formats/webm/webm_info_parser.h>
#include <packager/media/formats/webm/webm_tracks_parser.h>

namespace shaka {
namespace media {

WebMMediaParser::WebMMediaParser()
    : state_(kWaitingForInit), unknown_segment_size_(false) {}

WebMMediaParser::~WebMMediaParser() {}

void WebMMediaParser::Init(const InitCB& init_cb,
                           const NewMediaSampleCB& new_media_sample_cb,
                           const NewTextSampleCB&,
                           KeySource* decryption_key_source) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(!init_cb_);
  DCHECK(init_cb);
  DCHECK(new_media_sample_cb);

  ChangeState(kParsingHeaders);
  init_cb_ = init_cb;
  new_sample_cb_ = new_media_sample_cb;
  decryption_key_source_ = decryption_key_source;
  ignore_text_tracks_ = true;
}

bool WebMMediaParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);

  byte_queue_.Reset();
  bool result = true;
  if (cluster_parser_)
    result = cluster_parser_->Flush();
  if (state_ == kParsingClusters) {
    ChangeState(kParsingHeaders);
  }
  return result;
}

bool WebMMediaParser::Parse(const uint8_t* buf, int size) {
  DCHECK_NE(state_, kWaitingForInit);

  if (state_ == kError)
    return false;

  byte_queue_.Push(buf, size);

  int result = 0;
  int bytes_parsed = 0;
  const uint8_t* cur = NULL;
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

void WebMMediaParser::ChangeState(State new_state) {
  DVLOG(1) << "ChangeState() : " << state_ << " -> " << new_state;
  state_ = new_state;
}

int WebMMediaParser::ParseInfoAndTracks(const uint8_t* data, int size) {
  DVLOG(2) << "ParseInfoAndTracks()";
  DCHECK(data);
  DCHECK_GT(size, 0);

  const uint8_t* cur = data;
  int cur_size = size;
  int bytes_parsed = 0;

  int id;
  int64_t element_size;
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
    case kWebMIdTags:
    case kWebMIdAttachments:
      // TODO: Implement support for chapters.
      if (cur_size < (result + element_size)) {
        // We don't have the whole element yet. Signal we need more data.
        return 0;
      }
      // Skip the element.
      return result + element_size;
      break;
    case kWebMIdCluster:
      if (!cluster_parser_) {
        LOG(ERROR) << "Found Cluster element before Info.";
        return -1;
      }
      ChangeState(kParsingClusters);
      return 0;
      break;
    case kWebMIdSegment:
      // Segment of unknown size indicates live stream.
      if (element_size == kWebMUnknownSize)
        unknown_segment_size_ = true;
      // Just consume the segment header.
      return result;
      break;
    case kWebMIdInfo:
      // We've found the element we are looking for.
      break;
    default: {
      LOG(ERROR) << "Unexpected element ID 0x" << std::hex << id;
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

  WebMTracksParser tracks_parser(ignore_text_tracks_);
  result = tracks_parser.Parse(cur, cur_size);

  if (result <= 0)
    return result;

  bytes_parsed += result;

  double timecode_scale_in_us = info_parser.timecode_scale() / 1000.0;
  int64_t duration_in_us = info_parser.duration() * timecode_scale_in_us;

  std::shared_ptr<AudioStreamInfo> audio_stream_info =
      tracks_parser.audio_stream_info();
  if (audio_stream_info) {
    audio_stream_info->set_duration(duration_in_us);
  } else {
    VLOG(1) << "No audio track info found.";
  }

  std::shared_ptr<VideoStreamInfo> video_stream_info =
      tracks_parser.video_stream_info();
  if (video_stream_info) {
    video_stream_info->set_duration(duration_in_us);
  } else {
    VLOG(1) << "No video track info found.";
  }

  if (!FetchKeysIfNecessary(tracks_parser.audio_encryption_key_id(),
                            tracks_parser.video_encryption_key_id())) {
    return -1;
  }

  cluster_parser_.reset(new WebMClusterParser(
      info_parser.timecode_scale(), audio_stream_info, video_stream_info,
      tracks_parser.vp_config(),
      tracks_parser.GetAudioDefaultDuration(timecode_scale_in_us),
      tracks_parser.GetVideoDefaultDuration(timecode_scale_in_us),
      tracks_parser.text_tracks(), tracks_parser.ignored_tracks(),
      tracks_parser.audio_encryption_key_id(),
      tracks_parser.video_encryption_key_id(), new_sample_cb_, init_cb_,
      decryption_key_source_));

  return bytes_parsed;
}

int WebMMediaParser::ParseCluster(const uint8_t* data, int size) {
  if (!cluster_parser_)
    return -1;

  int bytes_parsed = cluster_parser_->Parse(data, size);
  if (bytes_parsed < 0)
    return bytes_parsed;

  bool cluster_ended = cluster_parser_->cluster_ended();
  if (cluster_ended) {
    ChangeState(kParsingHeaders);
  }

  return bytes_parsed;
}

bool WebMMediaParser::FetchKeysIfNecessary(
    const std::string& audio_encryption_key_id,
    const std::string& video_encryption_key_id) {
  if (audio_encryption_key_id.empty() && video_encryption_key_id.empty())
    return true;
  // An error will be returned later if the samples need to be decrypted.
  if (!decryption_key_source_)
    return true;

  Status status;
  if (!audio_encryption_key_id.empty()) {
    status.Update(decryption_key_source_->FetchKeys(
        EmeInitDataType::WEBM,
        std::vector<uint8_t>(audio_encryption_key_id.begin(),
                             audio_encryption_key_id.end())));
  }
  if (!video_encryption_key_id.empty()) {
    status.Update(decryption_key_source_->FetchKeys(
        EmeInitDataType::WEBM,
        std::vector<uint8_t>(video_encryption_key_id.begin(),
                             video_encryption_key_id.end())));
  }
  if (!status.ok()) {
    LOG(ERROR) << "Error fetching decryption keys: " << status;
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace shaka
