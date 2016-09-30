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
#include "packager/media/base/muxer_util.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/vp_codec_configuration_record.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/event/progress_listener.h"
#include "packager/third_party/libwebm/src/mkvmuxerutil.hpp"
#include "packager/third_party/libwebm/src/webmids.hpp"
#include "packager/version/version.h"

namespace shaka {
namespace media {
namespace webm {
namespace {
int64_t kTimecodeScale = 1000000;
int64_t kSecondsToNs = 1000000000L;
}  // namespace

Segmenter::Segmenter(const MuxerOptions& options)
    : reference_frame_timestamp_(0),
      options_(options),
      clear_lead_(0),
      enable_encryption_(false),
      info_(NULL),
      muxer_listener_(NULL),
      progress_listener_(NULL),
      progress_target_(0),
      accumulated_progress_(0),
      first_timestamp_(0),
      sample_duration_(0),
      segment_payload_pos_(0),
      cluster_length_in_time_scale_(0),
      segment_length_in_time_scale_(0),
      track_id_(0) {}

Segmenter::~Segmenter() {}

Status Segmenter::Initialize(std::unique_ptr<MkvWriter> writer,
                             StreamInfo* info,
                             ProgressListener* progress_listener,
                             MuxerListener* muxer_listener,
                             KeySource* encryption_key_source,
                             uint32_t max_sd_pixels,
                             double clear_lead_in_seconds) {
  muxer_listener_ = muxer_listener;
  info_ = info;
  clear_lead_ = clear_lead_in_seconds;

  // Use media duration as progress target.
  progress_target_ = info_->duration();
  progress_listener_ = progress_listener;

  segment_info_.Init();
  segment_info_.set_timecode_scale(kTimecodeScale);

  const std::string version = GetPackagerVersion();
  if (!version.empty()) {
    segment_info_.set_writing_app(
        (GetPackagerProjectUrl() + " version " + version).c_str());
  }

  if (options().single_segment) {
    // Set an initial duration so the duration element is written; will be
    // overwritten at the end.  This works because this is a float and floats
    // are always the same size.
    segment_info_.set_duration(1);
  }

  Status status;
  if (encryption_key_source) {
    status = InitializeEncryptor(encryption_key_source,
                                 max_sd_pixels);
    if (!status.ok())
      return status;
  }

  // Create the track info.
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

  return DoInitialize(std::move(writer));
}

Status Segmenter::Finalize() {
  Status status = WriteFrame(true /* write_duration */);
  if (!status.ok())
    return status;

  uint64_t duration =
      prev_sample_->pts() - first_timestamp_ + prev_sample_->duration();
  segment_info_.set_duration(FromBMFFTimescale(duration));
  return DoFinalize();
}

Status Segmenter::AddSample(scoped_refptr<MediaSample> sample) {
  if (sample_duration_ == 0) {
    first_timestamp_ = sample->pts();
    sample_duration_ = sample->duration();
    if (muxer_listener_)
      muxer_listener_->OnSampleDurationReady(sample_duration_);
  }

  UpdateProgress(sample->duration());

  // This writes frames in a delay.  Meaning that the previous frame is written
  // on this call to AddSample.  The current frame is stored until the next
  // call.  This is done to determine which frame is the last in a Cluster.
  // This first block determines if this is a new Cluster and writes the
  // previous frame first before creating the new Cluster.

  Status status;
  bool wrote_frame = false;
  bool new_segment = false;
  if (!cluster_) {
    status = NewSegment(sample->pts());
    new_segment = true;
    // First frame, so no previous frame to write.
    wrote_frame = true;
  } else if (segment_length_in_time_scale_ >=
             options_.segment_duration * info_->time_scale()) {
    if (sample->is_key_frame() || !options_.segment_sap_aligned) {
      status = WriteFrame(true /* write_duration */);
      status.Update(NewSegment(sample->pts()));
      new_segment = true;
      segment_length_in_time_scale_ = 0;
      cluster_length_in_time_scale_ = 0;
      wrote_frame = true;
    }
  } else if (cluster_length_in_time_scale_ >=
             options_.fragment_duration * info_->time_scale()) {
    if (sample->is_key_frame() || !options_.fragment_sap_aligned) {
      status = WriteFrame(true /* write_duration */);
      status.Update(NewSubsegment(sample->pts()));
      cluster_length_in_time_scale_ = 0;
      wrote_frame = true;
    }
  }
  if (!wrote_frame) {
    status = WriteFrame(false /* write_duration */);
  }
  if (!status.ok())
    return status;

  // Encrypt the frame.
  if (encryptor_) {
    // Don't enable encryption in the middle of a segment, i.e. only at the
    // first frame of a segment.
    if (new_segment && !enable_encryption_) {
      if (sample->pts() - first_timestamp_ >=
          clear_lead_ * info_->time_scale()) {
        enable_encryption_ = true;
        if (muxer_listener_)
          muxer_listener_->OnEncryptionStart();
      }
    }

    status = encryptor_->EncryptFrame(sample, enable_encryption_);
    if (!status.ok()) {
      LOG(ERROR) << "Error encrypting frame.";
      return status;
    }
  }

  // Add the sample to the durations even though we have not written the frame
  // yet.  This is needed to make sure we split Clusters at the correct point.
  // These are only used in this method.
  cluster_length_in_time_scale_ += sample->duration();
  segment_length_in_time_scale_ += sample->duration();

  prev_sample_ = sample;
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

  seek_head_.set_info_pos(writer->Position() - segment_payload_pos_);
  if (!segment_info_.Write(writer))
    return error_status;

  seek_head_.set_tracks_pos(writer->Position() - segment_payload_pos_);
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

    // The |StreamInfo::codec_config| field is stored using the MP4 format; we
    // need to convert it to the WebM format.
    VPCodecConfigurationRecord vp_config;
    if (!vp_config.ParseMP4(info->codec_config())) {
      return Status(error::INTERNAL_ERROR,
                    "Unable to parse VP9 codec configuration");
    }

    std::vector<uint8_t> codec_config;
    vp_config.WriteWebM(&codec_config);
    if (!track->SetCodecPrivate(codec_config.data(), codec_config.size())) {
      return Status(error::INTERNAL_ERROR,
                    "Private codec data required for VP9 streams");
    }
  } else {
    LOG(ERROR) << "Only VP8 and VP9 video codecs are supported.";
    return Status(error::UNIMPLEMENTED,
                  "Only VP8 and VP9 video codecs are supported.");
  }

  track->set_uid(info->track_id());
  if (!info->language().empty())
    track->set_language(info->language().c_str());
  track->set_type(mkvmuxer::Tracks::kVideo);
  track->set_width(info->width());
  track->set_height(info->height());
  track->set_display_height(info->height());
  track->set_display_width(info->width() * info->pixel_width() /
                           info->pixel_height());

  if (encryptor_)
    encryptor_->AddTrackInfo(track);

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
  if (!track->SetCodecPrivate(info->codec_config().data(),
                              info->codec_config().size())) {
    return Status(error::INTERNAL_ERROR,
                  "Private codec data required for audio streams");
  }

  track->set_uid(info->track_id());
  if (!info->language().empty())
    track->set_language(info->language().c_str());
  track->set_type(mkvmuxer::Tracks::kAudio);
  track->set_sample_rate(info->sampling_frequency());
  track->set_channels(info->num_channels());
  track->set_seek_pre_roll(info->seek_preroll_ns());
  track->set_codec_delay(info->codec_delay_ns());

  if (encryptor_)
    encryptor_->AddTrackInfo(track);

  tracks_.AddTrack(track, info->track_id());
  track_id_ = track->number();
  return Status::OK;
}

Status Segmenter::InitializeEncryptor(KeySource* key_source,
                                      uint32_t max_sd_pixels) {
  encryptor_.reset(new Encryptor());
  const KeySource::TrackType track_type =
      GetTrackTypeForEncryption(*info_, max_sd_pixels);
  if (track_type == KeySource::TrackType::TRACK_TYPE_UNKNOWN)
    return Status::OK;
  return encryptor_->Initialize(muxer_listener_, track_type, info_->codec(),
                                key_source, options_.webm_subsample_encryption);
}

Status Segmenter::WriteFrame(bool write_duration) {
  // Create a frame manually so we can create non-SimpleBlock frames.  This
  // is required to allow the frame duration to be added.  If the duration
  // is not set, then a SimpleBlock will still be written.
  mkvmuxer::Frame frame;

  if (!frame.Init(prev_sample_->data(), prev_sample_->data_size())) {
    return Status(error::MUXER_FAILURE,
                  "Error adding sample to segment: Frame::Init failed");
  }

  if (write_duration) {
    const uint64_t duration_ns =
        prev_sample_->duration() * kSecondsToNs / info_->time_scale();
    frame.set_duration(duration_ns);
  }
  frame.set_is_key(prev_sample_->is_key_frame());
  frame.set_timestamp(prev_sample_->pts() * kSecondsToNs / info_->time_scale());
  frame.set_track_number(track_id_);

  if (prev_sample_->side_data_size() > 0) {
    uint64_t block_add_id;
    // First 8 bytes of side_data is the BlockAddID element's value, which is
    // done to mimic ffmpeg behavior. See webm_cluster_parser.cc for details.
    CHECK_GT(prev_sample_->side_data_size(), sizeof(block_add_id));
    memcpy(&block_add_id, prev_sample_->side_data(), sizeof(block_add_id));
    if (!frame.AddAdditionalData(
            prev_sample_->side_data() + sizeof(block_add_id),
            prev_sample_->side_data_size() - sizeof(block_add_id),
            block_add_id)) {
      return Status(
          error::MUXER_FAILURE,
          "Error adding sample to segment: Frame::AddAditionalData Failed");
    }
  }

  if (!prev_sample_->is_key_frame() && !frame.CanBeSimpleBlock()) {
    const int64_t timestamp_ns =
        reference_frame_timestamp_ * kSecondsToNs / info_->time_scale();
    frame.set_reference_block_timestamp(timestamp_ns);
  }

  // GetRelativeTimecode will return -1 if the relative timecode is too large
  // to fit in the frame.
  if (cluster_->GetRelativeTimecode(frame.timestamp() /
                                    cluster_->timecode_scale()) < 0) {
    const double segment_duration =
        static_cast<double>(frame.timestamp()) / kSecondsToNs;
    LOG(ERROR) << "Error adding sample to segment: segment too large, "
               << segment_duration << " seconds.";
    return Status(error::MUXER_FAILURE,
                  "Error adding sample to segment: segment too large");
  }

  if (!cluster_->AddFrame(&frame)) {
    return Status(error::MUXER_FAILURE,
                  "Error adding sample to segment: Cluster::AddFrame failed");
  }

  // A reference frame is needed for non-keyframes.  Having a reference to the
  // previous block is good enough.
  // See libwebm Segment::AddGenericFrame
  reference_frame_timestamp_ = prev_sample_->pts();
  return Status::OK;
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
