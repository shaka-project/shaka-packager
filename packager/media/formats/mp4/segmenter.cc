// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/segmenter.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/status.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/encryption_config.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/id3_tag.h>
#include <packager/media/base/media_handler.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/base/muxer_util.h>
#include <packager/media/base/protection_system_specific_info.h>
#include <packager/media/base/stream_info.h>
#include <packager/media/event/progress_listener.h>
#include <packager/media/formats/mp4/box_definitions.h>
#include <packager/media/formats/mp4/fragmenter.h>
#include <packager/media/formats/mp4/key_frame_info.h>
#include <packager/status.h>
#include <packager/version/version.h>

namespace shaka {
namespace media {
namespace mp4 {

namespace {

int64_t Rescale(int64_t time_in_old_scale,
                int32_t old_scale,
                int32_t new_scale) {
  return static_cast<double>(time_in_old_scale) / old_scale * new_scale;
}

}  // namespace

Segmenter::Segmenter(const MuxerOptions& options,
                     std::unique_ptr<FileType> ftyp,
                     std::unique_ptr<Movie> moov)
    : options_(options),
      ftyp_(std::move(ftyp)),
      moov_(std::move(moov)),
      moof_(new MovieFragment()),
      fragment_buffer_(new BufferWriter()),
      sidx_(new SegmentIndex()) {}

Segmenter::~Segmenter() {}

Status Segmenter::Initialize(
    const std::vector<std::shared_ptr<const StreamInfo>>& streams,
    MuxerListener* muxer_listener,
    ProgressListener* progress_listener) {
  DCHECK_LT(0u, streams.size());
  muxer_listener_ = muxer_listener;
  progress_listener_ = progress_listener;
  moof_->header.sequence_number = 0;

  moof_->tracks.resize(streams.size());
  fragmenters_.resize(streams.size());
  stream_durations_.resize(streams.size());
  pending_events_.resize(streams.size());

  for (uint32_t i = 0; i < streams.size(); ++i) {
    moof_->tracks[i].header.track_id = i + 1;
    if (streams[i]->stream_type() == kStreamVideo) {
      // Use the first video stream as the reference stream (which is 1-based).
      if (sidx_->reference_id == 0)
        sidx_->reference_id = i + 1;
    }

    const EditList& edit_list = moov_->tracks[i].edit.list;
    int64_t edit_list_offset = 0;
    if (edit_list.edits.size() > 0) {
      DCHECK_EQ(edit_list.edits.size(), 1u);
      edit_list_offset = edit_list.edits.front().media_time;
    }

    fragmenters_[i].reset(
        new Fragmenter(streams[i], &moof_->tracks[i], edit_list_offset));
  }

  // Choose the first stream if there is no VIDEO.
  if (sidx_->reference_id == 0)
    sidx_->reference_id = 1;
  sidx_->timescale = streams[GetReferenceStreamId()]->time_scale();

  // Use media duration as progress target.
  progress_target_ = streams[GetReferenceStreamId()]->duration();

  // Use the reference stream's time scale as movie time scale.
  moov_->header.timescale = sidx_->timescale;
  moof_->header.sequence_number = 1;

  // Fill in version information.
  const std::string version = GetPackagerVersion();
  if (!version.empty()) {
    moov_->metadata.handler.handler_type = FOURCC_ID32;
    moov_->metadata.id3v2.language.code = "eng";

    Id3Tag id3_tag;
    id3_tag.AddPrivateFrame(GetPackagerProjectUrl(), version);
    CHECK(id3_tag.WriteToVector(&moov_->metadata.id3v2.id3v2_data));
  }
  return DoInitialize();
}

Status Segmenter::Finalize() {
  // With more than one track, some tracks may still hold queued data and the
  // last fragment is usually left unwritten, because tracks rarely end at
  // exactly the same timestamp. Writing a fragment unblocks the tracks waiting
  // on it, which may in turn produce another fragment, so alternate between
  // the two until there is nothing left.
  for (;;) {
    RETURN_IF_ERROR(DrainPendingEvents());
    bool wrote_fragment = false;
    RETURN_IF_ERROR(FlushRemainingFragments(&wrote_fragment));
    if (!wrote_fragment)
      break;
  }
  for (const std::deque<PendingEvent>& pending : pending_events_) {
    if (!pending.empty()) {
      return Status(error::MUXER_FAILURE,
                    "Not all media samples could be written to the output.");
    }
  }

  // Set movie duration. Note that the duration in mvhd, tkhd, mdhd should not
  // be touched, i.e. kept at 0. The updated moov box will be written to output
  // file for VOD and static live case only.
  moov_->extends.header.fragment_duration = 0;
  for (size_t i = 0; i < stream_durations_.size(); ++i) {
    int64_t duration =
        Rescale(stream_durations_[i], moov_->tracks[i].media.header.timescale,
                moov_->header.timescale);
    if (duration >
        static_cast<int64_t>(moov_->extends.header.fragment_duration))
      moov_->extends.header.fragment_duration = duration;
  }
  return DoFinalize();
}

Status Segmenter::AddSample(size_t stream_id, const MediaSample& sample) {
  DCHECK_LT(stream_id, fragmenters_.size());

  // A fragment covers every track, so a track that already reached the segment
  // boundary has to wait for the others before it can start the next fragment.
  // Queue its data until the current fragment has been written out.
  if (fragmenters_[stream_id]->fragment_finalized() ||
      !pending_events_[stream_id].empty()) {
    if (fragmenters_.size() == 1) {
      return Status(error::FRAGMENT_FINALIZED,
                    "Current fragment is finalized already.");
    }
    PendingEvent event;
    event.sample = sample.Clone();
    pending_events_[stream_id].push_back(std::move(event));
    return Status::OK;
  }
  return AddSampleInternal(stream_id, sample);
}

Status Segmenter::AddSampleInternal(size_t stream_id,
                                    const MediaSample& sample) {
  // Set default sample duration if it has not been set yet.
  if (moov_->extends.tracks[stream_id].default_sample_duration == 0) {
    moov_->extends.tracks[stream_id].default_sample_duration =
        sample.duration();
  }

  Fragmenter* fragmenter = fragmenters_[stream_id].get();
  DCHECK(!fragmenter->fragment_finalized());

  Status status = fragmenter->AddSample(sample);
  if (!status.ok())
    return status;

  // The duration of the first sample may have been adjusted, so use
  // the duration of the second sample instead.
  if (num_samples_ < 2) {
    sample_durations_[num_samples_] = sample.duration();
    num_samples_++;
  }
  stream_durations_[stream_id] += sample.duration();
  return Status::OK;
}

Status Segmenter::FinalizeSegment(size_t stream_id,
                                  const SegmentInfo& segment_info) {
  DCHECK_LT(stream_id, fragmenters_.size());

  if (fragmenters_.size() > 1 &&
      (fragmenters_[stream_id]->fragment_finalized() ||
       !pending_events_[stream_id].empty())) {
    // Keep this boundary behind whatever this track already has queued, so it
    // is replayed in the order it arrived.
    PendingEvent event;
    event.segment_info = segment_info;
    pending_events_[stream_id].push_back(std::move(event));
    return Status::OK;
  }

  RETURN_IF_ERROR(FinalizeSegmentInternal(stream_id, segment_info));
  return DrainPendingEvents();
}

Status Segmenter::DrainPendingEvents() {
  bool made_progress = true;
  while (made_progress) {
    made_progress = false;
    for (size_t i = 0; i < pending_events_.size(); ++i) {
      while (!pending_events_[i].empty() &&
             !fragmenters_[i]->fragment_finalized()) {
        const PendingEvent event = std::move(pending_events_[i].front());
        pending_events_[i].pop_front();
        made_progress = true;
        if (event.sample) {
          RETURN_IF_ERROR(AddSampleInternal(i, *event.sample));
        } else {
          // This may write out the fragment, which unblocks other streams; the
          // outer loop picks them up on the next pass.
          RETURN_IF_ERROR(FinalizeSegmentInternal(i, event.segment_info));
        }
      }
    }
  }
  return Status::OK;
}

Status Segmenter::FlushRemainingFragments(bool* wrote_fragment) {
  // Nothing else is coming at this point, so close whatever fragment each
  // track still has open and write out the tracks that hold data, instead of
  // waiting on tracks that have already run out of it.
  std::vector<size_t> stream_ids;
  for (size_t i = 0; i < fragmenters_.size(); ++i) {
    if (fragmenters_[i]->fragment_initialized())
      RETURN_IF_ERROR(fragmenters_[i]->FinalizeFragment());
    if (fragmenters_[i]->fragment_finalized())
      stream_ids.push_back(i);
  }
  *wrote_fragment = !stream_ids.empty();
  if (stream_ids.empty())
    return Status::OK;
  return WriteFragment(last_segment_info_, stream_ids);
}

Status Segmenter::FinalizeSegmentInternal(size_t stream_id,
                                          const SegmentInfo& segment_info) {
  last_segment_info_ = segment_info;

  if (segment_info.key_rotation_encryption_config) {
    FinalizeFragmentForKeyRotation(
        stream_id, segment_info.is_encrypted,
        *segment_info.key_rotation_encryption_config);
  }

  DCHECK_LT(stream_id, fragmenters_.size());
  Fragmenter* specified_fragmenter = fragmenters_[stream_id].get();
  DCHECK(specified_fragmenter);
  Status status = specified_fragmenter->FinalizeFragment();
  if (!status.ok())
    return status;

  // Check if all tracks are ready for fragmentation.
  for (const std::unique_ptr<Fragmenter>& fragmenter : fragmenters_) {
    if (!fragmenter->fragment_finalized())
      return Status::OK;
  }

  std::vector<size_t> stream_ids(fragmenters_.size());
  for (size_t i = 0; i < stream_ids.size(); ++i)
    stream_ids[i] = i;
  return WriteFragment(segment_info, stream_ids);
}

Status Segmenter::WriteFragment(const SegmentInfo& segment_info,
                                const std::vector<size_t>& stream_ids) {
  DCHECK(!stream_ids.empty());

  // 'moof' normally describes every track. A trailing fragment may cover only
  // the tracks that still had data when the others reached end of stream, so
  // assemble the box from just those tracks in that case.
  MovieFragment partial_moof;
  MovieFragment* moof = moof_.get();
  if (stream_ids.size() != fragmenters_.size()) {
    partial_moof.header = moof_->header;
    partial_moof.pssh = moof_->pssh;
    for (size_t stream_id : stream_ids)
      partial_moof.tracks.push_back(moof_->tracks[stream_id]);
    moof = &partial_moof;
  }

  MediaData mdat;
  // Data offset relative to 'moof': moof size + mdat header size.
  // The code will also update box sizes for moof and its child boxes.
  uint64_t data_offset = moof->ComputeSize() + mdat.HeaderSize();
  // 'traf' should follow 'mfhd' moof header box.
  uint64_t next_traf_position = moof->HeaderSize() + moof->header.box_size();
  for (size_t i = 0; i < moof->tracks.size(); ++i) {
    TrackFragment& traf = moof->tracks[i];
    if (traf.auxiliary_offset.offsets.size() > 0) {
      DCHECK_EQ(traf.auxiliary_offset.offsets.size(), 1u);
      DCHECK(!traf.sample_encryption.sample_encryption_entries.empty());

      next_traf_position += traf.box_size();
      // SampleEncryption 'senc' box should be the last box in 'traf'.
      // |auxiliary_offset| should point to the data of SampleEncryption.
      traf.auxiliary_offset.offsets[0] =
          next_traf_position - traf.sample_encryption.box_size() +
          traf.sample_encryption.HeaderSize() +
          sizeof(uint32_t);  // for sample count field in 'senc'
    }
    traf.runs[0].data_offset = data_offset + mdat.data_size;
    mdat.data_size +=
        static_cast<uint32_t>(fragmenters_[stream_ids[i]]->data()->Size());
  }

  // Generate segment reference. The reference stream may be absent from a
  // trailing fragment, in which case fall back to the first track present.
  const uint32_t reference_stream_id = GetReferenceStreamId();
  const bool has_reference_stream =
      std::find(stream_ids.begin(), stream_ids.end(), reference_stream_id) !=
      stream_ids.end();
  sidx_->references.resize(sidx_->references.size() + 1);
  fragmenters_[has_reference_stream ? reference_stream_id : stream_ids.front()]
      ->GenerateSegmentReference(
          &sidx_->references[sidx_->references.size() - 1]);
  sidx_->references[sidx_->references.size() - 1].referenced_size =
      data_offset + mdat.data_size;

  const uint64_t moof_start_offset = fragment_buffer_->Size();

  // Write the fragment to buffer.
  moof->Write(fragment_buffer_.get());
  mdat.WriteHeader(fragment_buffer_.get());

  bool first_key_frame = true;
  for (size_t stream_id : stream_ids) {
    const std::unique_ptr<Fragmenter>& fragmenter = fragmenters_[stream_id];
    // https://goo.gl/xcFus6 6. Trick play requirements
    // 6.10. If using fMP4, I-frame segments must include the 'moof' header
    // associated with the I-frame. It also implies that only the first key
    // frame can be included.
    if (!fragmenter->key_frame_infos().empty() && first_key_frame) {
      const KeyFrameInfo& key_frame_info =
          fragmenter->key_frame_infos().front();
      first_key_frame = false;
      key_frame_infos_.push_back(
          {key_frame_info.timestamp, moof_start_offset,
           fragment_buffer_->Size() - moof_start_offset + key_frame_info.size});
    }
    fragment_buffer_->AppendBuffer(*fragmenter->data());
  }

  // Increase sequence_number for next fragment.
  ++moof_->header.sequence_number;

  for (std::unique_ptr<Fragmenter>& fragmenter : fragmenters_)
    fragmenter->ClearFragmentFinalized();

  if (segment_info.is_chunk) {
    // Finalize the completed chunk for the LL-DASH case.
    RETURN_IF_ERROR(DoFinalizeChunk(segment_info.segment_number));
  }

  if (!segment_info.is_subsegment || segment_info.is_final_chunk_in_seg) {
    // Finalize the segment.
    Status status = DoFinalizeSegment(segment_info.segment_number);

    // Reset segment information to initial state.
    sidx_->references.clear();
    key_frame_infos_.clear();
    return status;
  }
  return Status::OK;
}

int32_t Segmenter::GetReferenceTimeScale() const {
  return moov_->header.timescale;
}

double Segmenter::GetDuration() const {
  int64_t duration = moov_->extends.header.fragment_duration;
  if (duration == 0) {
    // Handling the case where this is not properly initialized.
    return 0.0;
  }
  return static_cast<double>(duration) / moov_->header.timescale;
}

void Segmenter::UpdateProgress(uint64_t progress) {
  accumulated_progress_ += progress;

  if (!progress_listener_)
    return;
  if (progress_target_ == 0)
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

void Segmenter::SetComplete() {
  if (!progress_listener_)
    return;
  progress_listener_->OnProgress(1.0);
}

uint32_t Segmenter::GetReferenceStreamId() {
  DCHECK(sidx_);
  return sidx_->reference_id - 1;
}

void Segmenter::FinalizeFragmentForKeyRotation(
    size_t stream_id,
    bool fragment_encrypted,
    const EncryptionConfig& encryption_config) {
  if (options_.mp4_params.include_pssh_in_stream) {
    moof_->pssh.clear();
    const auto& key_system_info = encryption_config.key_system_info;
    for (const ProtectionSystemSpecificInfo& system : key_system_info) {
      if (system.psshs.empty())
        continue;
      ProtectionSystemSpecificHeader pssh;
      pssh.raw_box = system.psshs;
      moof_->pssh.push_back(pssh);
    }
  } else {
    LOG(WARNING)
        << "Key rotation and no pssh in stream may not work well together.";
  }

  // Skip the following steps if the current fragment is not going to be
  // encrypted. 'pssh' box needs to be included in the fragment, which is
  // performed above, regardless of whether the fragment is encrypted. This is
  // necessary for two reasons: 1) Requesting keys before reaching encrypted
  // content avoids playback delay due to license requests; 2) In Chrome, CDM
  // must be initialized before starting the playback and CDM can only be
  // initialized with a valid 'pssh'.
  if (!fragment_encrypted)
    return;

  DCHECK_LT(stream_id, moof_->tracks.size());
  TrackFragment& traf = moof_->tracks[stream_id];
  traf.sample_group_descriptions.resize(traf.sample_group_descriptions.size() +
                                        1);
  SampleGroupDescription& sample_group_description =
      traf.sample_group_descriptions.back();
  sample_group_description.grouping_type = FOURCC_seig;

  sample_group_description.cenc_sample_encryption_info_entries.resize(1);
  CencSampleEncryptionInfoEntry& sample_group_entry =
      sample_group_description.cenc_sample_encryption_info_entries.back();
  sample_group_entry.is_protected = 1;
  sample_group_entry.per_sample_iv_size = encryption_config.per_sample_iv_size;
  sample_group_entry.constant_iv = encryption_config.constant_iv;
  sample_group_entry.crypt_byte_block = encryption_config.crypt_byte_block;
  sample_group_entry.skip_byte_block = encryption_config.skip_byte_block;
  sample_group_entry.key_id = encryption_config.key_id;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
