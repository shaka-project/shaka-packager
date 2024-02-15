// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/webm/webm_tracks_parser.h>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/str_format.h>

#include <packager/media/base/timestamp.h>
#include <packager/media/formats/webm/webm_constants.h>
#include <packager/media/formats/webm/webm_content_encodings.h>

namespace shaka {
namespace media {

static TextKind CodecIdToTextKind(const std::string& codec_id) {
  if (codec_id == kWebMCodecSubtitles)
    return kTextSubtitles;

  if (codec_id == kWebMCodecCaptions)
    return kTextCaptions;

  if (codec_id == kWebMCodecDescriptions)
    return kTextDescriptions;

  if (codec_id == kWebMCodecMetadata)
    return kTextMetadata;

  return kTextNone;
}

static int64_t PrecisionCappedDefaultDuration(
    const double timecode_scale_in_us,
    const int64_t duration_in_ns) {
  if (duration_in_ns <= 0)
    return kNoTimestamp;

  int64_t mult = duration_in_ns / 1000;
  mult /= timecode_scale_in_us;
  if (mult == 0)
    return kNoTimestamp;

  mult = static_cast<double>(mult) * timecode_scale_in_us;
  return mult;
}

WebMTracksParser::WebMTracksParser(bool ignore_text_tracks)
    : track_type_(-1),
      track_num_(-1),
      seek_preroll_(-1),
      codec_delay_(-1),
      default_duration_(-1),
      audio_track_num_(-1),
      audio_default_duration_(-1),
      video_track_num_(-1),
      video_default_duration_(-1),
      ignore_text_tracks_(ignore_text_tracks),
      audio_client_(),
      video_client_() {
}

WebMTracksParser::~WebMTracksParser() {}

int WebMTracksParser::Parse(const uint8_t* buf, int size) {
  track_type_ =-1;
  track_num_ = -1;
  default_duration_ = -1;
  track_name_.clear();
  track_language_.clear();
  audio_track_num_ = -1;
  audio_default_duration_ = -1;
  audio_stream_info_ = nullptr;
  video_track_num_ = -1;
  video_default_duration_ = -1;
  video_stream_info_ = nullptr;
  text_tracks_.clear();
  ignored_tracks_.clear();

  WebMListParser parser(kWebMIdTracks, this);
  int result = parser.Parse(buf, size);

  if (result <= 0)
    return result;

  // For now we do all or nothing parsing.
  return parser.IsParsingComplete() ? result : 0;
}

int64_t WebMTracksParser::GetAudioDefaultDuration(
    const double timecode_scale_in_us) const {
  return PrecisionCappedDefaultDuration(timecode_scale_in_us,
                                        audio_default_duration_);
}

int64_t WebMTracksParser::GetVideoDefaultDuration(
    const double timecode_scale_in_us) const {
  return PrecisionCappedDefaultDuration(timecode_scale_in_us,
                                        video_default_duration_);
}

WebMParserClient* WebMTracksParser::OnListStart(int id) {
  if (id == kWebMIdContentEncodings) {
    DCHECK(!track_content_encodings_client_.get());
    track_content_encodings_client_.reset(new WebMContentEncodingsClient());
    return track_content_encodings_client_->OnListStart(id);
  }

  if (id == kWebMIdTrackEntry) {
    track_type_ = -1;
    track_num_ = -1;
    default_duration_ = -1;
    track_name_.clear();
    track_language_.clear();
    codec_id_ = "";
    codec_private_.clear();
    audio_client_.Reset();
    video_client_.Reset();
    return this;
  }

  if (id == kWebMIdAudio)
    return &audio_client_;

  if (id == kWebMIdVideo)
    return &video_client_;

  return this;
}

bool WebMTracksParser::OnListEnd(int id) {
  if (id == kWebMIdContentEncodings) {
    DCHECK(track_content_encodings_client_.get());
    return track_content_encodings_client_->OnListEnd(id);
  }

  if (id == kWebMIdTrackEntry) {
    if (track_type_ == -1 || track_num_ == -1) {
      LOG(ERROR) << "Missing TrackEntry data for "
                 << " TrackType " << track_type_ << " TrackNum " << track_num_;
      return false;
    }

    if (track_type_ != kWebMTrackTypeAudio &&
        track_type_ != kWebMTrackTypeVideo &&
        track_type_ != kWebMTrackTypeSubtitlesOrCaptions &&
        track_type_ != kWebMTrackTypeDescriptionsOrMetadata) {
      LOG(ERROR) << "Unexpected TrackType " << track_type_;
      return false;
    }

    TextKind text_track_kind = kTextNone;
    if (track_type_ == kWebMTrackTypeSubtitlesOrCaptions) {
      text_track_kind = CodecIdToTextKind(codec_id_);
      if (text_track_kind == kTextNone) {
        LOG(ERROR) << "Missing TrackEntry CodecID"
                   << " TrackNum " << track_num_;
        return false;
      }

      if (text_track_kind != kTextSubtitles &&
          text_track_kind != kTextCaptions) {
        LOG(ERROR) << "Wrong TrackEntry CodecID"
                   << " TrackNum " << track_num_;
        return false;
      }
    } else if (track_type_ == kWebMTrackTypeDescriptionsOrMetadata) {
      text_track_kind = CodecIdToTextKind(codec_id_);
      if (text_track_kind == kTextNone) {
        LOG(ERROR) << "Missing TrackEntry CodecID"
                   << " TrackNum " << track_num_;
        return false;
      }

      if (text_track_kind != kTextDescriptions &&
          text_track_kind != kTextMetadata) {
        LOG(ERROR) << "Wrong TrackEntry CodecID"
                   << " TrackNum " << track_num_;
        return false;
      }
    }

    std::string encryption_key_id;
    if (track_content_encodings_client_) {
      DCHECK(!track_content_encodings_client_->content_encodings().empty());
      // If we have multiple ContentEncoding in one track. Always choose the
      // key id in the first ContentEncoding as the key id of the track.
      encryption_key_id = track_content_encodings_client_->
          content_encodings()[0]->encryption_key_id();
    }

    if (track_type_ == kWebMTrackTypeAudio) {
      if (audio_track_num_ == -1) {
        audio_track_num_ = track_num_;
        audio_encryption_key_id_ = encryption_key_id;

        if (default_duration_ == 0) {
          LOG(ERROR) << "Illegal 0ns audio TrackEntry "
                        "DefaultDuration";
          return false;
        }
        audio_default_duration_ = default_duration_;

        DCHECK(!audio_stream_info_);
        audio_stream_info_ = audio_client_.GetAudioStreamInfo(
            audio_track_num_, codec_id_, codec_private_, seek_preroll_,
            codec_delay_, track_language_, !audio_encryption_key_id_.empty());
        if (!audio_stream_info_)
          return false;
      } else {
        DLOG(INFO) << "Ignoring audio track " << track_num_;
        ignored_tracks_.insert(track_num_);
      }
    } else if (track_type_ == kWebMTrackTypeVideo) {
      if (video_track_num_ == -1) {
        video_track_num_ = track_num_;
        video_encryption_key_id_ = encryption_key_id;

        if (default_duration_ == 0) {
          LOG(ERROR) << "Illegal 0ns video TrackEntry "
                        "DefaultDuration";
          return false;
        }
        video_default_duration_ = default_duration_;

        DCHECK(!video_stream_info_);
        video_stream_info_ = video_client_.GetVideoStreamInfo(
            video_track_num_, codec_id_, codec_private_,
            !video_encryption_key_id_.empty());
        if (!video_stream_info_)
          return false;

        if (codec_id_ == "V_VP8" || codec_id_ == "V_VP9") {
          vp_config_ = video_client_.GetVpCodecConfig(codec_private_);
          const double kNanosecondsPerSecond = 1000000000.0;
          if (codec_id_ == "V_VP9" &&
              (!vp_config_.is_level_set() || vp_config_.level() == 0)) {
            vp_config_.SetVP9Level(
                video_stream_info_->width(), video_stream_info_->height(),
                video_default_duration_ / kNanosecondsPerSecond);
          }
        }

      } else {
        DLOG(INFO) << "Ignoring video track " << track_num_;
        ignored_tracks_.insert(track_num_);
      }
    } else if (track_type_ == kWebMTrackTypeSubtitlesOrCaptions ||
               track_type_ == kWebMTrackTypeDescriptionsOrMetadata) {
      if (ignore_text_tracks_) {
        DLOG(INFO) << "Ignoring text track " << track_num_;
        ignored_tracks_.insert(track_num_);
      } else {
        std::string track_num = absl::StrFormat("%d", track_num_);
        text_tracks_[track_num_] = TextTrackConfig(
            text_track_kind, track_name_, track_language_, track_num);
      }
    } else {
      LOG(ERROR) << "Unexpected TrackType " << track_type_;
      return false;
    }

    track_type_ = -1;
    track_num_ = -1;
    default_duration_ = -1;
    track_name_.clear();
    track_language_.clear();
    codec_id_ = "";
    codec_private_.clear();
    track_content_encodings_client_.reset();

    audio_client_.Reset();
    video_client_.Reset();
    return true;
  }

  return true;
}

bool WebMTracksParser::OnUInt(int id, int64_t val) {
  int64_t* dst = NULL;

  switch (id) {
    case kWebMIdTrackNumber:
      dst = &track_num_;
      break;
    case kWebMIdTrackType:
      dst = &track_type_;
      break;
    case kWebMIdSeekPreRoll:
      dst = &seek_preroll_;
      break;
    case kWebMIdCodecDelay:
      dst = &codec_delay_;
      break;
    case kWebMIdDefaultDuration:
      dst = &default_duration_;
      break;
    default:
      return true;
  }

  if (*dst != -1) {
    LOG(ERROR) << "Multiple values for id " << std::hex << id << " specified";
    return false;
  }

  *dst = val;
  return true;
}

bool WebMTracksParser::OnFloat(int /*id*/, double /*val*/) {
  return true;
}

bool WebMTracksParser::OnBinary(int id, const uint8_t* data, int size) {
  if (id == kWebMIdCodecPrivate) {
    if (!codec_private_.empty()) {
      LOG(ERROR) << "Multiple CodecPrivate fields in a track.";
      return false;
    }
    codec_private_.assign(data, data + size);
    return true;
  }
  return true;
}

bool WebMTracksParser::OnString(int id, const std::string& str) {
  if (id == kWebMIdCodecID) {
    if (!codec_id_.empty()) {
      LOG(ERROR) << "Multiple CodecID fields in a track";
      return false;
    }

    codec_id_ = str;
    return true;
  }

  if (id == kWebMIdName) {
    track_name_ = str;
    return true;
  }

  if (id == kWebMIdLanguage) {
    track_language_ = str;
    return true;
  }

  return true;
}

}  // namespace media
}  // namespace shaka
