// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/segmenter.h"

#include <algorithm>

#include "packager/base/stl_util.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/media_stream.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/event/progress_listener.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/key_rotation_fragmenter.h"

namespace edash_packager {
namespace media {
namespace mp4 {

namespace {

// Generate 64bit IV by default.
const size_t kDefaultIvSize = 8u;
const size_t kCencKeyIdSize = 16u;

// The version of cenc implemented here. CENC 4.
const int kCencSchemeVersion = 0x00010000;

uint64_t Rescale(uint64_t time_in_old_scale,
                 uint32_t old_scale,
                 uint32_t new_scale) {
  return static_cast<double>(time_in_old_scale) / old_scale * new_scale;
}

void GenerateSinf(const EncryptionKey& encryption_key,
                  FourCC old_type,
                  ProtectionSchemeInfo* sinf) {
  sinf->format.format = old_type;
  sinf->type.type = FOURCC_CENC;
  sinf->type.version = kCencSchemeVersion;
  sinf->info.track_encryption.is_encrypted = true;
  sinf->info.track_encryption.default_iv_size =
      encryption_key.iv.empty() ? kDefaultIvSize : encryption_key.iv.size();
  sinf->info.track_encryption.default_kid = encryption_key.key_id;
}

void GenerateEncryptedSampleEntry(const EncryptionKey& encryption_key,
                                  double clear_lead_in_seconds,
                                  SampleDescription* description) {
  DCHECK(description);
  if (description->type == kVideo) {
    DCHECK_EQ(1u, description->video_entries.size());

    // Add a second entry for clear content if needed.
    if (clear_lead_in_seconds > 0)
      description->video_entries.push_back(description->video_entries[0]);

    // Convert the first entry to an encrypted entry.
    VideoSampleEntry& entry = description->video_entries[0];
    GenerateSinf(encryption_key, entry.format, &entry.sinf);
    entry.format = FOURCC_ENCV;
  } else {
    DCHECK_EQ(kAudio, description->type);
    DCHECK_EQ(1u, description->audio_entries.size());

    // Add a second entry for clear content if needed.
    if (clear_lead_in_seconds > 0)
      description->audio_entries.push_back(description->audio_entries[0]);

    // Convert the first entry to an encrypted entry.
    AudioSampleEntry& entry = description->audio_entries[0];
    GenerateSinf(encryption_key, entry.format, &entry.sinf);
    entry.format = FOURCC_ENCA;
  }
}

void GenerateEncryptedSampleEntryForKeyRotation(
    double clear_lead_in_seconds,
    SampleDescription* description) {
  // Fill encrypted sample entry with default key.
  EncryptionKey encryption_key;
  encryption_key.key_id.assign(kCencKeyIdSize, 0);
  GenerateEncryptedSampleEntry(
      encryption_key, clear_lead_in_seconds, description);
}

uint8_t GetNaluLengthSize(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamVideo)
    return 0;
  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  return video_stream_info.nalu_length_size();
}

KeySource::TrackType GetTrackTypeForEncryption(const StreamInfo& stream_info,
                                               uint32_t max_sd_pixels) {
  if (stream_info.stream_type() == kStreamAudio)
    return KeySource::TRACK_TYPE_AUDIO;

  DCHECK_EQ(kStreamVideo, stream_info.stream_type());
  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  uint32_t pixels = video_stream_info.width() * video_stream_info.height();
  return (pixels > max_sd_pixels) ? KeySource::TRACK_TYPE_HD
                                  : KeySource::TRACK_TYPE_SD;
}

}  // namespace

Segmenter::Segmenter(const MuxerOptions& options,
                     scoped_ptr<FileType> ftyp,
                     scoped_ptr<Movie> moov)
    : options_(options),
      ftyp_(ftyp.Pass()),
      moov_(moov.Pass()),
      moof_(new MovieFragment()),
      fragment_buffer_(new BufferWriter()),
      sidx_(new SegmentIndex()),
      segment_initialized_(false),
      end_of_segment_(false),
      muxer_listener_(NULL),
      progress_listener_(NULL),
      progress_target_(0),
      accumulated_progress_(0),
      sample_duration_(0u) {
}

Segmenter::~Segmenter() { STLDeleteElements(&fragmenters_); }

Status Segmenter::Initialize(const std::vector<MediaStream*>& streams,
                             MuxerListener* muxer_listener,
                             ProgressListener* progress_listener,
                             KeySource* encryption_key_source,
                             uint32_t max_sd_pixels,
                             double clear_lead_in_seconds,
                             double crypto_period_duration_in_seconds) {
  DCHECK_LT(0u, streams.size());
  muxer_listener_ = muxer_listener;
  progress_listener_ = progress_listener;
  moof_->header.sequence_number = 0;

  moof_->tracks.resize(streams.size());
  segment_durations_.resize(streams.size());
  fragmenters_.resize(streams.size());
  for (uint32_t i = 0; i < streams.size(); ++i) {
    stream_map_[streams[i]] = i;
    moof_->tracks[i].header.track_id = i + 1;
    if (streams[i]->info()->stream_type() == kStreamVideo) {
      // Use the first video stream as the reference stream (which is 1-based).
      if (sidx_->reference_id == 0)
        sidx_->reference_id = i + 1;
    }
    if (!encryption_key_source) {
      fragmenters_[i] = new Fragmenter(&moof_->tracks[i]);
      continue;
    }

    uint8_t nalu_length_size = GetNaluLengthSize(*streams[i]->info());
    KeySource::TrackType track_type =
        GetTrackTypeForEncryption(*streams[i]->info(), max_sd_pixels);
    SampleDescription& description =
        moov_->tracks[i].media.information.sample_table.description;

    const bool key_rotation_enabled = crypto_period_duration_in_seconds != 0;
    if (key_rotation_enabled) {
      GenerateEncryptedSampleEntryForKeyRotation(clear_lead_in_seconds,
                                                 &description);

      fragmenters_[i] = new KeyRotationFragmenter(
          moof_.get(),
          &moof_->tracks[i],
          encryption_key_source,
          track_type,
          crypto_period_duration_in_seconds * streams[i]->info()->time_scale(),
          clear_lead_in_seconds * streams[i]->info()->time_scale(),
          nalu_length_size);
      continue;
    }

    scoped_ptr<EncryptionKey> encryption_key(new EncryptionKey());
    Status status =
        encryption_key_source->GetKey(track_type, encryption_key.get());
    if (!status.ok())
      return status;

    GenerateEncryptedSampleEntry(
        *encryption_key, clear_lead_in_seconds, &description);

    // One and only one pssh box is needed.
    if (moov_->pssh.empty()) {
      moov_->pssh.resize(1);
      moov_->pssh[0].raw_box = encryption_key->pssh;

      // Also only one default key id.
      if (muxer_listener_) {
        muxer_listener_->OnEncryptionInfoReady(
            encryption_key_source->UUID(), encryption_key_source->SystemName(),
            encryption_key->key_id, encryption_key->pssh);
      }
    }

    fragmenters_[i] = new EncryptingFragmenter(
        &moof_->tracks[i],
        encryption_key.Pass(),
        clear_lead_in_seconds * streams[i]->info()->time_scale(),
        nalu_length_size);
  }

  // Choose the first stream if there is no VIDEO.
  if (sidx_->reference_id == 0)
    sidx_->reference_id = 1;
  sidx_->timescale = streams[GetReferenceStreamId()]->info()->time_scale();

  // Use media duration as progress target.
  progress_target_ = streams[GetReferenceStreamId()]->info()->duration();

  // Use the reference stream's time scale as movie time scale.
  moov_->header.timescale = sidx_->timescale;
  moof_->header.sequence_number = 1;
  return DoInitialize();
}

Status Segmenter::Finalize() {
  end_of_segment_ = true;
  for (std::vector<Fragmenter*>::iterator it = fragmenters_.begin();
       it != fragmenters_.end();
       ++it) {
    Status status = FinalizeFragment(*it);
    if (!status.ok())
      return status;
  }

  // Set tracks and moov durations.
  // Note that the updated moov box will be written to output file for VOD case
  // only.
  for (std::vector<Track>::iterator track = moov_->tracks.begin();
       track != moov_->tracks.end();
       ++track) {
    track->header.duration = Rescale(track->media.header.duration,
                                     track->media.header.timescale,
                                     moov_->header.timescale);
    if (track->header.duration > moov_->header.duration)
      moov_->header.duration = track->header.duration;
  }
  moov_->extends.header.fragment_duration = moov_->header.duration;

  return DoFinalize();
}

Status Segmenter::AddSample(const MediaStream* stream,
                            scoped_refptr<MediaSample> sample) {
  // Find the fragmenter for this stream.
  DCHECK(stream);
  DCHECK(stream_map_.find(stream) != stream_map_.end());
  uint32_t stream_id = stream_map_[stream];
  Fragmenter* fragmenter = fragmenters_[stream_id];

  // Set default sample duration if it has not been set yet.
  if (moov_->extends.tracks[stream_id].default_sample_duration == 0) {
    moov_->extends.tracks[stream_id].default_sample_duration =
        sample->duration();
  }

  if (!segment_initialized_) {
    InitializeSegment();
    segment_initialized_ = true;
  }

  if (fragmenter->fragment_finalized()) {
    return Status(error::FRAGMENT_FINALIZED,
                  "Current fragment is finalized already.");
  }

  bool finalize_fragment = false;
  if (fragmenter->fragment_duration() >=
      options_.fragment_duration * stream->info()->time_scale()) {
    if (sample->is_key_frame() || !options_.fragment_sap_aligned) {
      finalize_fragment = true;
    }
  }
  if (segment_durations_[stream_id] >=
      options_.segment_duration * stream->info()->time_scale()) {
    if (sample->is_key_frame() || !options_.segment_sap_aligned) {
      end_of_segment_ = true;
      finalize_fragment = true;
    }
  }

  Status status;
  if (finalize_fragment) {
    status = FinalizeFragment(fragmenter);
    if (!status.ok())
      return status;
  }

  status = fragmenter->AddSample(sample);
  if (!status.ok())
    return status;

  if (sample_duration_ == 0)
    sample_duration_ = sample->duration();
  moov_->tracks[stream_id].media.header.duration += sample->duration();
  segment_durations_[stream_id] += sample->duration();
  return Status::OK;
}

uint32_t Segmenter::GetReferenceTimeScale() const {
  return moov_->header.timescale;
}

double Segmenter::GetDuration() const {
  if (moov_->header.timescale == 0) {
    // Handling the case where this is not properly initialized.
    return 0.0;
  }

  return static_cast<double>(moov_->header.duration) / moov_->header.timescale;
}

void Segmenter::UpdateProgress(uint64_t progress) {
  accumulated_progress_ += progress;

  if (!progress_listener_) return;
  if (progress_target_ == 0) return;
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

void Segmenter::SetComplete() {
  if (!progress_listener_) return;
  progress_listener_->OnProgress(1.0);
}

void Segmenter::InitializeSegment() {
  sidx_->references.clear();
  end_of_segment_ = false;
  std::vector<uint64_t>::iterator it = segment_durations_.begin();
  for (; it != segment_durations_.end(); ++it)
    *it = 0;
}

Status Segmenter::FinalizeSegment() {
  segment_initialized_ = false;
  return DoFinalizeSegment();
}

uint32_t Segmenter::GetReferenceStreamId() {
  DCHECK(sidx_);
  return sidx_->reference_id - 1;
}

Status Segmenter::FinalizeFragment(Fragmenter* fragmenter) {
  fragmenter->FinalizeFragment();

  // Check if all tracks are ready for fragmentation.
  for (std::vector<Fragmenter*>::iterator it = fragmenters_.begin();
       it != fragmenters_.end();
       ++it) {
    if (!(*it)->fragment_finalized())
      return Status::OK;
  }

  MediaData mdat;
  // Fill in data offsets. Data offset base is moof size + mdat box size.
  // (mdat is still empty, mdat size is the same as mdat box size).
  uint64_t base = moof_->ComputeSize() + mdat.ComputeSize();
  for (size_t i = 0; i < moof_->tracks.size(); ++i) {
    TrackFragment& traf = moof_->tracks[i];
    Fragmenter* fragmenter = fragmenters_[i];
    if (fragmenter->aux_data()->Size() > 0) {
      traf.auxiliary_offset.offsets[0] += base;
      base += fragmenter->aux_data()->Size();
    }
    traf.runs[0].data_offset += base;
    base += fragmenter->data()->Size();
  }

  // Generate segment reference.
  sidx_->references.resize(sidx_->references.size() + 1);
  fragmenters_[GetReferenceStreamId()]->GenerateSegmentReference(
      &sidx_->references[sidx_->references.size() - 1]);
  sidx_->references[sidx_->references.size() - 1].referenced_size = base;

  // Write the fragment to buffer.
  moof_->Write(fragment_buffer_.get());

  for (size_t i = 0; i < moof_->tracks.size(); ++i) {
    Fragmenter* fragmenter = fragmenters_[i];
    mdat.data_size =
        fragmenter->aux_data()->Size() + fragmenter->data()->Size();
    mdat.Write(fragment_buffer_.get());
    if (fragmenter->aux_data()->Size()) {
      fragment_buffer_->AppendBuffer(*fragmenter->aux_data());
    }
    fragment_buffer_->AppendBuffer(*fragmenter->data());
  }

  // Increase sequence_number for next fragment.
  ++moof_->header.sequence_number;

  if (end_of_segment_)
    return FinalizeSegment();

  return Status::OK;
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager
