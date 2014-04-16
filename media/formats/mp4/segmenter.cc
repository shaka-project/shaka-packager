// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/formats/mp4/segmenter.h"

#include <algorithm>

#include "base/stl_util.h"
#include "media/base/aes_encryptor.h"
#include "media/base/buffer_writer.h"
#include "media/base/encryption_key_source.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"
#include "media/base/muxer_options.h"
#include "media/base/video_stream_info.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/fragmenter.h"

namespace media {
namespace mp4 {

namespace {

// Generate 64bit IV by default.
const size_t kDefaultIvSize = 8u;

// The version of cenc implemented here. CENC 4.
const int kCencSchemeVersion = 0x00010000;

uint64 Rescale(uint64 time_in_old_scale, uint32 old_scale, uint32 new_scale) {
  return static_cast<double>(time_in_old_scale) / old_scale * new_scale;
}

scoped_ptr<AesCtrEncryptor> CreateEncryptor(
    const EncryptionKey& encryption_key) {
  scoped_ptr<AesCtrEncryptor> encryptor(new AesCtrEncryptor());
  const bool initialized =
      encryption_key.iv.empty()
          ? encryptor->InitializeWithRandomIv(encryption_key.key,
                                              kDefaultIvSize)
          : encryptor->InitializeWithIv(encryption_key.key, encryption_key.iv);
  if (!initialized) {
    LOG(ERROR) << "Failed to the initialize encryptor.";
    return scoped_ptr<AesCtrEncryptor>();
  }
  return encryptor.Pass();
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
      end_of_segment_(false) {}

Segmenter::~Segmenter() { STLDeleteElements(&fragmenters_); }

Status Segmenter::Initialize(const std::vector<MediaStream*>& streams,
                             EncryptionKeySource* encryption_key_source,
                             EncryptionKeySource::TrackType track_type,
                             double clear_lead_in_seconds) {
  DCHECK_LT(0u, streams.size());
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
    if (encryption_key_source) {
      SampleDescription& description =
          moov_->tracks[i].media.information.sample_table.description;

      DCHECK(track_type == EncryptionKeySource::TRACK_TYPE_SD ||
             track_type == EncryptionKeySource::TRACK_TYPE_HD);

      EncryptionKey encryption_key;
      Status status = encryption_key_source->GetKey(
          description.type == kAudio ? EncryptionKeySource::TRACK_TYPE_AUDIO
                                     : track_type,
          &encryption_key);
      if (!status.ok())
        return status;

      GenerateEncryptedSampleEntry(
          encryption_key, clear_lead_in_seconds, &description);

      // We need one and only one pssh box.
      if (moov_->pssh.empty()) {
        moov_->pssh.resize(1);
        moov_->pssh[0].raw_box = encryption_key.pssh;
      }

      encryptor = CreateEncryptor(encryption_key);
      if (!encryptor)
        return Status(error::MUXER_FAILURE, "Failed to create the encryptor.");
    }
    fragmenters_[i] = new Fragmenter(
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

  return DoFinalize();
}

Status Segmenter::AddSample(const MediaStream* stream,
                            scoped_refptr<MediaSample> sample) {
  // Find the fragmenter for this stream.
  DCHECK(stream);
  DCHECK(stream_map_.find(stream) != stream_map_.end());
  uint32 stream_id = stream_map_[stream];
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

  moov_->tracks[stream_id].media.header.duration += sample->duration();
  segment_durations_[stream_id] += sample->duration();
  return Status::OK;
}

uint32 Segmenter::GetReferenceTimeScale() const {
  return moov_->header.timescale;
}

double Segmenter::GetDuration() const {
  if (moov_->header.timescale == 0) {
    // Handling the case where this is not properly initialized.
    return 0.0;
  }

  return static_cast<double>(moov_->header.duration) / moov_->header.timescale;
}

void Segmenter::InitializeSegment() {
  sidx_->references.clear();
  end_of_segment_ = false;
  std::vector<uint64>::iterator it = segment_durations_.begin();
  for (; it != segment_durations_.end(); ++it)
    *it = 0;
}

Status Segmenter::FinalizeSegment() {
  segment_initialized_ = false;
  return DoFinalizeSegment();
}

uint32 Segmenter::GetReferenceStreamId() {
  DCHECK(sidx_);
  return sidx_->reference_id - 1;
}

void Segmenter::InitializeFragments() {
  ++moof_->header.sequence_number;
  for (std::vector<Fragmenter*>::iterator it = fragmenters_.begin();
       it != fragmenters_.end();
       ++it) {
    (*it)->InitializeFragment();
  }
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
  uint64 base = moof_->ComputeSize() + mdat.ComputeSize();
  for (uint i = 0; i < moof_->tracks.size(); ++i) {
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

  for (uint i = 0; i < moof_->tracks.size(); ++i) {
    Fragmenter* fragmenter = fragmenters_[i];
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
