// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/segmenter.h"

#include "packager/base/time/time.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/media_stream.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/event/progress_listener.h"
#include "packager/third_party/libwebm/src/mkvmuxerutil.hpp"
#include "packager/third_party/libwebm/src/webmids.hpp"

namespace edash_packager {
namespace media {
namespace webm {
namespace {
int64_t kTimecodeScale = 1000000;
int64_t kSecondsToNs = 1000000000L;
}  // namespace

Segmenter::Segmenter(const MuxerOptions& options)
    : options_(options),
      info_(NULL),
      muxer_listener_(NULL),
      progress_listener_(NULL),
      progress_target_(0),
      accumulated_progress_(0),
      total_duration_(0),
      sample_duration_(0),
      segment_payload_pos_(0),
      cluster_length_sec_(0),
      segment_length_sec_(0),
      track_id_(0) {}

Segmenter::~Segmenter() {}

Status Segmenter::Initialize(scoped_ptr<MkvWriter> writer,
                             StreamInfo* info,
                             ProgressListener* progress_listener,
                             MuxerListener* muxer_listener,
                             KeySource* encryption_key_source) {
  muxer_listener_ = muxer_listener;
  info_ = info;

  // Use media duration as progress target.
  progress_target_ = info_->duration();
  progress_listener_ = progress_listener;

  const std::string version_string =
      "https://github.com/google/edash-packager version " +
      options().packager_version_string;

  segment_info_.Init();
  segment_info_.set_timecode_scale(kTimecodeScale);
  segment_info_.set_writing_app(version_string.c_str());
  if (options().single_segment) {
    // Set an initial duration so the duration element is written; will be
    // overwritten at the end.
    segment_info_.set_duration(1);
  }

  // Create the track info.
  Status status;
  switch (info_->stream_type()) {
    case kStreamVideo:
      status = CreateVideoTrack(static_cast<VideoStreamInfo*>(info_));
      break;
    case kStreamAudio:
      status = CreateAudioTrack(static_cast<AudioStreamInfo*>(info_));
      break;
    default:
      NOTIMPLEMENTED() << "Not implemented for stream type: "
                       << info_->stream_type();
      status = Status(error::UNIMPLEMENTED, "Not implemented for stream type");
  }
  if (!status.ok())
    return status;

  return DoInitialize(writer.Pass());
}

Status Segmenter::Finalize() {
  segment_info_.set_duration(FromBMFFTimescale(total_duration_));
  return DoFinalize();
}

Status Segmenter::AddSample(scoped_refptr<MediaSample> sample) {
  if (sample_duration_ == 0) {
    sample_duration_ = sample->duration();
    if (muxer_listener_)
      muxer_listener_->OnSampleDurationReady(sample_duration_);
  }

  UpdateProgress(sample->duration());

  // Create a new cluster if needed.
  Status status;
  if (!cluster_) {
    status = NewSegment(sample->pts());
  } else if (segment_length_sec_ >= options_.segment_duration) {
    if (sample->is_key_frame() || !options_.segment_sap_aligned) {
      status = NewSegment(sample->pts());
      segment_length_sec_ = 0;
      cluster_length_sec_ = 0;
    }
  } else if (cluster_length_sec_ >= options_.fragment_duration) {
    if (sample->is_key_frame() || !options_.fragment_sap_aligned) {
      status = NewSubsegment(sample->pts());
      cluster_length_sec_ = 0;
    }
  }
  if (!status.ok())
    return status;

  const int64_t time_ns =
      sample->pts() * kSecondsToNs / info_->time_scale();
  if (!cluster_->AddFrame(sample->data(), sample->data_size(), track_id_,
                          time_ns, sample->is_key_frame())) {
    LOG(ERROR) << "Error adding sample to segment.";
    return Status(error::FILE_FAILURE, "Error adding sample to segment.");
  }
  const double duration_sec =
      static_cast<double>(sample->duration()) / info_->time_scale();
  cluster_length_sec_ += duration_sec;
  segment_length_sec_ += duration_sec;
  total_duration_ += sample->duration();

  return Status::OK;
}

float Segmenter::GetDuration() const {
  return static_cast<float>(segment_info_.duration()) *
         segment_info_.timecode_scale() / kSecondsToNs;
}

uint64_t Segmenter::FromBMFFTimescale(uint64_t time_timescale) {
  // Convert the time from BMFF time_code to WebM timecode scale.
  const int64_t time_ns =
      kSecondsToNs * time_timescale / info_->time_scale();
  return time_ns / segment_info_.timecode_scale();
}

uint64_t Segmenter::FromWebMTimecode(uint64_t time_webm_timecode) {
  // Convert the time to BMFF time_code from WebM timecode scale.
  const int64_t time_ns = time_webm_timecode * segment_info_.timecode_scale();
  return time_ns * info_->time_scale() / kSecondsToNs;
}

Status Segmenter::WriteSegmentHeader(uint64_t file_size, MkvWriter* writer) {
  Status error_status(error::FILE_FAILURE, "Error writing segment header.");

  if (!WriteEbmlHeader(writer))
    return error_status;

  if (WriteID(writer, mkvmuxer::kMkvSegment) != 0)
    return error_status;

  const uint64_t segment_size_size = 8;
  segment_payload_pos_ = writer->Position() + segment_size_size;
  if (file_size > 0) {
    // We want the size of the segment element, so subtract the header.
    if (WriteUIntSize(writer, file_size - segment_payload_pos_,
                      segment_size_size) != 0)
      return error_status;
    if (!seek_head_.Write(writer))
      return error_status;
  } else {
    if (SerializeInt(writer, mkvmuxer::kEbmlUnknownValue, segment_size_size) !=
        0)
      return error_status;
    // We don't know the header size, so write a placeholder.
    if (!seek_head_.WriteVoid(writer))
      return error_status;
  }

  if (!segment_info_.Write(writer))
    return error_status;

  if (!tracks_.Write(writer))
    return error_status;

  return Status::OK;
}

Status Segmenter::SetCluster(uint64_t start_webm_timecode,
                             uint64_t position,
                             MkvWriter* writer) {
  const uint64_t scale = segment_info_.timecode_scale();
  cluster_.reset(new mkvmuxer::Cluster(start_webm_timecode, position, scale));
  cluster_->Init(writer);
  return Status::OK;
}

void Segmenter::UpdateProgress(uint64_t progress) {
  accumulated_progress_ += progress;
  if (!progress_listener_ || progress_target_ == 0)
    return;
  // It might happen that accumulated progress exceeds progress_target due to
  // computation errors, e.g. rounding error. Cap it so it never reports > 100%
  // progress.
  if (accumulated_progress_ >= progress_target_) {
    progress_listener_->OnProgress(1.0);
  } else {
    progress_listener_->OnProgress(static_cast<double>(accumulated_progress_) /
                                   progress_target_);
  }
}

Status Segmenter::CreateVideoTrack(VideoStreamInfo* info) {
  // The seed is only used to create a UID which we overwrite later.
  unsigned int seed = 0;
  mkvmuxer::VideoTrack* track = new mkvmuxer::VideoTrack(&seed);
  if (!track)
    return Status(error::INTERNAL_ERROR, "Failed to create video track.");

  if (info->codec() == kCodecVP8) {
    track->set_codec_id(mkvmuxer::Tracks::kVp8CodecId);
  } else if (info->codec() == kCodecVP9) {
    track->set_codec_id(mkvmuxer::Tracks::kVp9CodecId);
  } else {
    LOG(ERROR) << "Only VP8 and VP9 video codec is supported.";
    return Status(error::UNIMPLEMENTED,
                  "Only VP8 and VP9 video codecs are supported.");
  }

  track->set_uid(info->track_id());
  track->set_type(mkvmuxer::Tracks::kVideo);
  track->set_width(info->width());
  track->set_height(info->height());
  track->set_display_height(info->height());
  track->set_display_width(info->width() * info->pixel_width() /
                           info->pixel_height());
  track->set_language(info->language().c_str());

  tracks_.AddTrack(track, info->track_id());
  track_id_ = track->number();
  return Status::OK;
}

Status Segmenter::CreateAudioTrack(AudioStreamInfo* info) {
  // The seed is only used to create a UID which we overwrite later.
  unsigned int seed = 0;
  mkvmuxer::AudioTrack* track = new mkvmuxer::AudioTrack(&seed);
  if (!track)
    return Status(error::INTERNAL_ERROR, "Failed to create audio track.");

  if (info->codec() == kCodecOpus) {
    track->set_codec_id(mkvmuxer::Tracks::kOpusCodecId);
  } else if (info->codec() == kCodecVorbis) {
    track->set_codec_id(mkvmuxer::Tracks::kVorbisCodecId);
  } else {
    LOG(ERROR) << "Only Vorbis and Opus audio codec is supported.";
    return Status(error::UNIMPLEMENTED,
                  "Only Vorbis and Opus audio codecs are supported.");
  }

  track->set_uid(info->track_id());
  track->set_language(info->language().c_str());
  track->set_type(mkvmuxer::Tracks::kAudio);
  track->set_sample_rate(info->sampling_frequency());
  track->set_channels(info->num_channels());

  tracks_.AddTrack(track, info->track_id());
  track_id_ = track->number();
  return Status::OK;
}

}  // namespace webm
}  // namespace media
}  // namespace edash_packager
