// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/mp4_segmenter.h"

#include <algorithm>

#include "base/stl_util.h"
#include "media/base/buffer_writer.h"
#include "media/base/encryptor_source.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"
#include "media/base/muxer_options.h"
#include "media/base/video_stream_info.h"
#include "media/mp4/box_definitions.h"
#include "media/mp4/mp4_fragmenter.h"

namespace {
uint64 Rescale(uint64 time_in_old_scale, uint32 old_scale, uint32 new_scale) {
  return static_cast<double>(time_in_old_scale) / old_scale * new_scale;
}
}  // namespace

namespace media {
namespace mp4 {

MP4Segmenter::MP4Segmenter(const MuxerOptions& options,
                           scoped_ptr<FileType> ftyp,
                           scoped_ptr<Movie> moov)
    : options_(options),
      fragment_buffer_(new BufferWriter()),
      ftyp_(ftyp.Pass()),
      moov_(moov.Pass()),
      moof_(new MovieFragment()),
      sidx_(new SegmentIndex()),
      segment_initialized_(false),
      end_of_segment_(false) {}

MP4Segmenter::~MP4Segmenter() { STLDeleteElements(&fragmenters_); }

Status MP4Segmenter::Initialize(EncryptorSource* encryptor_source,
                                double clear_lead_in_seconds,
                                const std::vector<MediaStream*>& streams) {
  DCHECK_LT(0, streams.size());
  moof_->header.sequence_number = 0;

  moof_->tracks.resize(streams.size());
  segment_durations_.resize(streams.size());
  fragmenters_.resize(streams.size());
  for (uint32 i = 0; i < streams.size(); ++i) {
    stream_map_[streams[i]] = i;
    moof_->tracks[i].header.track_id = i + 1;
    uint8 nalu_length_size = 0;
    if (streams[i]->info()->stream_type() == kStreamVideo) {
      VideoStreamInfo* video =
          static_cast<VideoStreamInfo*>(streams[i]->info().get());
      nalu_length_size = video->nalu_length_size();
      // We use the first video stream as the reference stream.
      if (sidx_->reference_id == 0)
        sidx_->reference_id = i + 1;
    }
    scoped_ptr<AesCtrEncryptor> encryptor;
    if (encryptor_source) {
      encryptor = encryptor_source->CreateEncryptor();
      if (!encryptor)
        return Status(error::MUXER_FAILURE, "Failed to create the encryptor.");
    }
    fragmenters_[i] = new MP4Fragmenter(
        &moof_->tracks[i],
        encryptor.Pass(),
        clear_lead_in_seconds * streams[i]->info()->time_scale(),
        nalu_length_size,
        options_.normalize_presentation_timestamp);
  }

  // Choose the first stream if there is no VIDEO.
  if (sidx_->reference_id == 0)
    sidx_->reference_id = 1;
  sidx_->timescale = streams[GetReferenceStreamId()]->info()->time_scale();

  // Use the reference stream's time scale as movie time scale.
  moov_->header.timescale = sidx_->timescale;
  InitializeFragments();
  return Status::OK;
}

Status MP4Segmenter::Finalize() {
  end_of_segment_ = true;
  for (std::vector<MP4Fragmenter*>::iterator it = fragmenters_.begin();
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

  return Status::OK;
}

Status MP4Segmenter::AddSample(const MediaStream* stream,
                               scoped_refptr<MediaSample> sample) {
  // Find the fragmenter for this stream.
  DCHECK(stream);
  DCHECK(stream_map_.find(stream) != stream_map_.end());
  uint32 stream_id = stream_map_[stream];
  MP4Fragmenter* fragmenter = fragmenters_[stream_id];

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

  moov_->tracks[stream_id].media.header.duration += sample->duration();
  segment_durations_[stream_id] += sample->duration();
  return Status::OK;
}

uint32 MP4Segmenter::GetReferenceTimeScale() const {
  return moov_->header.timescale;
}

double MP4Segmenter::GetDuration() const {
  if (moov_->header.timescale == 0) {
    // Handling the case where this is not properly initialized.
    return 0.0;
  }

  return static_cast<double>(moov_->header.duration) / moov_->header.timescale;
}

void MP4Segmenter::InitializeSegment() {
  sidx_->references.clear();
  end_of_segment_ = false;
  std::vector<uint64>::iterator it = segment_durations_.begin();
  for (; it != segment_durations_.end(); ++it)
    *it = 0;
}

Status MP4Segmenter::FinalizeSegment() {
  segment_initialized_ = false;
  return Status::OK;
}

uint32 MP4Segmenter::GetReferenceStreamId() {
  DCHECK(sidx_);
  return sidx_->reference_id - 1;
}

void MP4Segmenter::InitializeFragments() {
  ++moof_->header.sequence_number;
  for (std::vector<MP4Fragmenter*>::iterator it = fragmenters_.begin();
       it != fragmenters_.end();
       ++it) {
    (*it)->InitializeFragment();
  }
}

Status MP4Segmenter::FinalizeFragment(MP4Fragmenter* fragmenter) {
  fragmenter->FinalizeFragment();

  // Check if all tracks are ready for fragmentation.
  for (std::vector<MP4Fragmenter*>::iterator it = fragmenters_.begin();
       it != fragmenters_.end();
       ++it) {
    if (!(*it)->fragment_finalized())
      return Status::OK;
  }

  MediaData mdat;
  // Fill in data offsets. Data offset base is moof size + mdat box size.
  // (mdat is still empty, mdat size is the same as mdat box size).
  uint64 base = moof_->ComputeSize() + mdat.ComputeSize();
  for (uint i = 0; i < moof_->tracks.size(); ++i) {
    TrackFragment& traf = moof_->tracks[i];
    MP4Fragmenter* fragmenter = fragmenters_[i];
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

  for (uint i = 0; i < moof_->tracks.size(); ++i) {
    MP4Fragmenter* fragmenter = fragmenters_[i];
    mdat.data_size =
        fragmenter->aux_data()->Size() + fragmenter->data()->Size();
    mdat.Write(fragment_buffer_.get());
    if (fragmenter->aux_data()->Size()) {
      fragment_buffer_->AppendBuffer(*fragmenter->aux_data());
    }
    fragment_buffer_->AppendBuffer(*fragmenter->data());
  }

  InitializeFragments();

  if (end_of_segment_)
    return FinalizeSegment();

  return Status::OK;
}

}  // namespace mp4
}  // namespace media
