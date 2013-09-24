// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_cluster_parser.h"

#include <vector>

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "media/base/buffers.h"
#include "media/base/decrypt_config.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_crypto_helpers.h"

namespace media {

WebMClusterParser::TextTrackIterator::TextTrackIterator(
    const TextTrackMap& text_track_map) :
    iterator_(text_track_map.begin()),
    iterator_end_(text_track_map.end()) {
}

WebMClusterParser::TextTrackIterator::TextTrackIterator(
    const TextTrackIterator& rhs) :
    iterator_(rhs.iterator_),
    iterator_end_(rhs.iterator_end_) {
}

WebMClusterParser::TextTrackIterator::~TextTrackIterator() {
}

bool WebMClusterParser::TextTrackIterator::operator()(
  int* track_num,
  const BufferQueue** buffers) {
  if (iterator_ == iterator_end_) {
    *track_num = 0;
    *buffers = NULL;

    return false;
  }

  *track_num = iterator_->first;
  *buffers = &iterator_->second.buffers();

  ++iterator_;
  return true;
}

WebMClusterParser::WebMClusterParser(
    int64 timecode_scale, int audio_track_num, int video_track_num,
    const WebMTracksParser::TextTracks& text_tracks,
    const std::set<int64>& ignored_tracks,
    const std::string& audio_encryption_key_id,
    const std::string& video_encryption_key_id,
    const LogCB& log_cb)
    : timecode_multiplier_(timecode_scale / 1000.0),
      ignored_tracks_(ignored_tracks),
      audio_encryption_key_id_(audio_encryption_key_id),
      video_encryption_key_id_(video_encryption_key_id),
      parser_(kWebMIdCluster, this),
      last_block_timecode_(-1),
      block_data_size_(-1),
      block_duration_(-1),
      block_add_id_(-1),
      block_additional_data_size_(-1),
      cluster_timecode_(-1),
      cluster_start_time_(kNoTimestamp()),
      cluster_ended_(false),
      audio_(audio_track_num, false),
      video_(video_track_num, true),
      log_cb_(log_cb) {
  for (WebMTracksParser::TextTracks::const_iterator it = text_tracks.begin();
       it != text_tracks.end();
       ++it) {
    text_track_map_.insert(std::make_pair(it->first, Track(it->first, false)));
  }
}

WebMClusterParser::~WebMClusterParser() {}

void WebMClusterParser::Reset() {
  last_block_timecode_ = -1;
  cluster_timecode_ = -1;
  cluster_start_time_ = kNoTimestamp();
  cluster_ended_ = false;
  parser_.Reset();
  audio_.Reset();
  video_.Reset();
  ResetTextTracks();
}

int WebMClusterParser::Parse(const uint8* buf, int size) {
  audio_.Reset();
  video_.Reset();
  ResetTextTracks();

  int result = parser_.Parse(buf, size);

  if (result < 0) {
    cluster_ended_ = false;
    return result;
  }

  cluster_ended_ = parser_.IsParsingComplete();
  if (cluster_ended_) {
    // If there were no buffers in this cluster, set the cluster start time to
    // be the |cluster_timecode_|.
    if (cluster_start_time_ == kNoTimestamp()) {
      DCHECK_GT(cluster_timecode_, -1);
      cluster_start_time_ = base::TimeDelta::FromMicroseconds(
          cluster_timecode_ * timecode_multiplier_);
    }

    // Reset the parser if we're done parsing so that
    // it is ready to accept another cluster on the next
    // call.
    parser_.Reset();

    last_block_timecode_ = -1;
    cluster_timecode_ = -1;
  }

  return result;
}

WebMClusterParser::TextTrackIterator
WebMClusterParser::CreateTextTrackIterator() const {
  return TextTrackIterator(text_track_map_);
}

WebMParserClient* WebMClusterParser::OnListStart(int id) {
  if (id == kWebMIdCluster) {
    cluster_timecode_ = -1;
    cluster_start_time_ = kNoTimestamp();
  } else if (id == kWebMIdBlockGroup) {
    block_data_.reset();
    block_data_size_ = -1;
    block_duration_ = -1;
  } else if (id == kWebMIdBlockAdditions) {
    block_add_id_ = -1;
    block_additional_data_.reset();
    block_additional_data_size_ = -1;
  }

  return this;
}

bool WebMClusterParser::OnListEnd(int id) {
  if (id != kWebMIdBlockGroup)
    return true;

  // Make sure the BlockGroup actually had a Block.
  if (block_data_size_ == -1) {
    MEDIA_LOG(log_cb_) << "Block missing from BlockGroup.";
    return false;
  }

  bool result = ParseBlock(false, block_data_.get(), block_data_size_,
                           block_additional_data_.get(),
                           block_additional_data_size_, block_duration_);
  block_data_.reset();
  block_data_size_ = -1;
  block_duration_ = -1;
  block_add_id_ = -1;
  block_additional_data_.reset();
  block_additional_data_size_ = -1;
  return result;
}

bool WebMClusterParser::OnUInt(int id, int64 val) {
  int64* dst;
  switch (id) {
    case kWebMIdTimecode:
      dst = &cluster_timecode_;
      break;
    case kWebMIdBlockDuration:
      dst = &block_duration_;
      break;
    case kWebMIdBlockAddID:
      dst = &block_add_id_;
      break;
    default:
      return true;
  }
  if (*dst != -1)
    return false;
  *dst = val;
  return true;
}

bool WebMClusterParser::ParseBlock(bool is_simple_block, const uint8* buf,
                                   int size, const uint8* additional,
                                   int additional_size, int duration) {
  if (size < 4)
    return false;

  // Return an error if the trackNum > 127. We just aren't
  // going to support large track numbers right now.
  if (!(buf[0] & 0x80)) {
    MEDIA_LOG(log_cb_) << "TrackNumber over 127 not supported";
    return false;
  }

  int track_num = buf[0] & 0x7f;
  int timecode = buf[1] << 8 | buf[2];
  int flags = buf[3] & 0xff;
  int lacing = (flags >> 1) & 0x3;

  if (lacing) {
    MEDIA_LOG(log_cb_) << "Lacing " << lacing << " is not supported yet.";
    return false;
  }

  // Sign extend negative timecode offsets.
  if (timecode & 0x8000)
    timecode |= ~0xffff;

  const uint8* frame_data = buf + 4;
  int frame_size = size - (frame_data - buf);
  return OnBlock(is_simple_block, track_num, timecode, duration, flags,
                 frame_data, frame_size, additional, additional_size);
}

bool WebMClusterParser::OnBinary(int id, const uint8* data, int size) {
  switch (id) {
    case kWebMIdSimpleBlock:
      return ParseBlock(true, data, size, NULL, -1, -1);

    case kWebMIdBlock:
      if (block_data_) {
        MEDIA_LOG(log_cb_) << "More than 1 Block in a BlockGroup is not "
                              "supported.";
        return false;
      }
      block_data_.reset(new uint8[size]);
      memcpy(block_data_.get(), data, size);
      block_data_size_ = size;
      return true;

    case kWebMIdBlockAdditional: {
      uint64 block_add_id = base::HostToNet64(block_add_id_);
      if (block_additional_data_) {
        // TODO(vigneshv): Technically, more than 1 BlockAdditional is allowed
        // as per matroska spec. But for now we don't have a use case to
        // support parsing of such files. Take a look at this again when such a
        // case arises.
        MEDIA_LOG(log_cb_) << "More than 1 BlockAdditional in a BlockGroup is "
                              "not supported.";
        return false;
      }
      // First 8 bytes of side_data in DecoderBuffer is the BlockAddID
      // element's value in Big Endian format. This is done to mimic ffmpeg
      // demuxer's behavior.
      block_additional_data_size_ = size + sizeof(block_add_id);
      block_additional_data_.reset(new uint8[block_additional_data_size_]);
      memcpy(block_additional_data_.get(), &block_add_id,
             sizeof(block_add_id));
      memcpy(block_additional_data_.get() + 8, data, size);
      return true;
    }

    default:
      return true;
  }
}

bool WebMClusterParser::OnBlock(bool is_simple_block, int track_num,
                                int timecode,
                                int  block_duration,
                                int flags,
                                const uint8* data, int size,
                                const uint8* additional, int additional_size) {
  DCHECK_GE(size, 0);
  if (cluster_timecode_ == -1) {
    MEDIA_LOG(log_cb_) << "Got a block before cluster timecode.";
    return false;
  }

  // TODO(acolwell): Should relative negative timecode offsets be rejected?  Or
  // only when the absolute timecode is negative?  See http://crbug.com/271794
  if (timecode < 0) {
    MEDIA_LOG(log_cb_) << "Got a block with negative timecode offset "
                       << timecode;
    return false;
  }

  if (last_block_timecode_ != -1 && timecode < last_block_timecode_) {
    MEDIA_LOG(log_cb_)
        << "Got a block with a timecode before the previous block.";
    return false;
  }

  Track* track = NULL;
  std::string encryption_key_id;
  if (track_num == audio_.track_num()) {
    track = &audio_;
    encryption_key_id = audio_encryption_key_id_;
  } else if (track_num == video_.track_num()) {
    track = &video_;
    encryption_key_id = video_encryption_key_id_;
  } else if (ignored_tracks_.find(track_num) != ignored_tracks_.end()) {
    return true;
  } else if (Track* const text_track = FindTextTrack(track_num)) {
    if (is_simple_block)  // BlockGroup is required for WebVTT cues
      return false;
    if (block_duration < 0)  // not specified
      return false;
    track = text_track;
  } else {
    MEDIA_LOG(log_cb_) << "Unexpected track number " << track_num;
    return false;
  }

  last_block_timecode_ = timecode;

  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      (cluster_timecode_ + timecode) * timecode_multiplier_);

  // The first bit of the flags is set when a SimpleBlock contains only
  // keyframes. If this is a Block, then inspection of the payload is
  // necessary to determine whether it contains a keyframe or not.
  // http://www.matroska.org/technical/specs/index.html
  bool is_keyframe =
      is_simple_block ? (flags & 0x80) != 0 : track->IsKeyframe(data, size);

  scoped_refptr<StreamParserBuffer> buffer =
      StreamParserBuffer::CopyFrom(data, size, additional, additional_size,
                                   is_keyframe);

  // Every encrypted Block has a signal byte and IV prepended to it. Current
  // encrypted WebM request for comments specification is here
  // http://wiki.webmproject.org/encryption/webm-encryption-rfc
  if (!encryption_key_id.empty()) {
    scoped_ptr<DecryptConfig> config(WebMCreateDecryptConfig(
        data, size,
        reinterpret_cast<const uint8*>(encryption_key_id.data()),
        encryption_key_id.size()));
    if (!config)
      return false;
    buffer->set_decrypt_config(config.Pass());
  }

  buffer->set_timestamp(timestamp);
  if (cluster_start_time_ == kNoTimestamp())
    cluster_start_time_ = timestamp;

  if (block_duration >= 0) {
    buffer->set_duration(base::TimeDelta::FromMicroseconds(
        block_duration * timecode_multiplier_));
  }

  return track->AddBuffer(buffer);
}

WebMClusterParser::Track::Track(int track_num, bool is_video)
    : track_num_(track_num),
      is_video_(is_video) {
}

WebMClusterParser::Track::~Track() {}

bool WebMClusterParser::Track::AddBuffer(
    const scoped_refptr<StreamParserBuffer>& buffer) {
  DVLOG(2) << "AddBuffer() : " << track_num_
           << " ts " << buffer->timestamp().InSecondsF()
           << " dur " << buffer->duration().InSecondsF()
           << " kf " << buffer->IsKeyframe()
           << " size " << buffer->data_size();

  buffers_.push_back(buffer);
  return true;
}

void WebMClusterParser::Track::Reset() {
  buffers_.clear();
}

bool WebMClusterParser::Track::IsKeyframe(const uint8* data, int size) const {
  // For now, assume that all blocks are keyframes for datatypes other than
  // video. This is a valid assumption for Vorbis, WebVTT, & Opus.
  if (!is_video_)
    return true;

  // Make sure the block is big enough for the minimal keyframe header size.
  if (size < 7)
    return false;

  // The LSb of the first byte must be a 0 for a keyframe.
  // http://tools.ietf.org/html/rfc6386 Section 19.1
  if ((data[0] & 0x01) != 0)
    return false;

  // Verify VP8 keyframe startcode.
  // http://tools.ietf.org/html/rfc6386 Section 19.1
  if (data[3] != 0x9d || data[4] != 0x01 || data[5] != 0x2a)
    return false;

  return true;
}

void WebMClusterParser::ResetTextTracks() {
  for (TextTrackMap::iterator it = text_track_map_.begin();
       it != text_track_map_.end();
       ++it) {
    it->second.Reset();
  }
}

WebMClusterParser::Track*
WebMClusterParser::FindTextTrack(int track_num) {
  const TextTrackMap::iterator it = text_track_map_.find(track_num);

  if (it == text_track_map_.end())
    return NULL;

  return &it->second;
}

}  // namespace media
