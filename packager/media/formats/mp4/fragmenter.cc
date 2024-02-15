// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/fragmenter.h>

#include <algorithm>
#include <limits>

#include <absl/log/check.h>

#include <packager/macros/status.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/formats/mp4/box_definitions.h>
#include <packager/media/formats/mp4/key_frame_info.h>

namespace shaka {
namespace media {
namespace mp4 {

namespace {
const int64_t kInvalidTime = std::numeric_limits<int64_t>::max();

int64_t GetSeekPreroll(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamAudio)
    return 0;
  const AudioStreamInfo& audio_stream_info =
      static_cast<const AudioStreamInfo&>(stream_info);
  return audio_stream_info.seek_preroll_ns();
}

void NewSampleEncryptionEntry(const DecryptConfig& decrypt_config,
                              bool use_constant_iv,
                              TrackFragment* traf) {
  SampleEncryption& sample_encryption = traf->sample_encryption;
  SampleEncryptionEntry sample_encryption_entry;
  if (!use_constant_iv)
    sample_encryption_entry.initialization_vector = decrypt_config.iv();
  sample_encryption_entry.subsamples = decrypt_config.subsamples();
  sample_encryption.sample_encryption_entries.push_back(
      sample_encryption_entry);
  traf->auxiliary_size.sample_info_sizes.push_back(
      sample_encryption_entry.ComputeSize());
}

}  // namespace

Fragmenter::Fragmenter(std::shared_ptr<const StreamInfo> stream_info,
                       TrackFragment* traf,
                       int64_t edit_list_offset)
    : stream_info_(std::move(stream_info)),
      traf_(traf),
      edit_list_offset_(edit_list_offset),
      seek_preroll_(GetSeekPreroll(*stream_info_)),
      earliest_presentation_time_(kInvalidTime),
      first_sap_time_(kInvalidTime) {
  DCHECK(stream_info_);
  DCHECK(traf);
}

Fragmenter::~Fragmenter() {}

Status Fragmenter::AddSample(const MediaSample& sample) {
  const int64_t pts = sample.pts();
  const int64_t dts = sample.dts();
  const int64_t duration = sample.duration();
  if (duration == 0)
    LOG(WARNING) << "Unexpected sample with zero duration @ dts " << dts;

  if (!fragment_initialized_)
    RETURN_IF_ERROR(InitializeFragment(dts));

  if (sample.side_data_size() > 0)
    LOG(WARNING) << "MP4 samples do not support side data. Side data ignored.";

  // Fill in sample parameters. It will be optimized later.
  traf_->runs[0].sample_sizes.push_back(
      static_cast<uint32_t>(sample.data_size()));
  traf_->runs[0].sample_durations.push_back(duration);
  traf_->runs[0].sample_flags.push_back(
      sample.is_key_frame() ? TrackFragmentHeader::kUnset
                            : TrackFragmentHeader::kNonKeySampleMask);

  if (sample.decrypt_config()) {
    NewSampleEncryptionEntry(
        *sample.decrypt_config(),
        !stream_info_->encryption_config().constant_iv.empty(), traf_);
  }

  if (stream_info_->stream_type() == StreamType::kStreamVideo &&
      sample.is_key_frame()) {
    key_frame_infos_.push_back({pts, data_->Size(), sample.data_size()});
  }

  data_->AppendArray(sample.data(), sample.data_size());

  traf_->runs[0].sample_composition_time_offsets.push_back(pts - dts);
  if (pts != dts)
    traf_->runs[0].flags |= TrackFragmentRun::kSampleCompTimeOffsetsPresentMask;

  // Exclude the part of sample with negative pts out of duration calculation as
  // they are not presented.
  if (pts < 0) {
    const int64_t end_pts = pts + duration;
    if (end_pts > 0) {
      // Include effective presentation duration.
      fragment_duration_ += end_pts;

      earliest_presentation_time_ = 0;
      if (sample.is_key_frame())
        first_sap_time_ = 0;
    }
  } else {
    fragment_duration_ += duration;

    if (earliest_presentation_time_ > pts)
      earliest_presentation_time_ = pts;

    if (sample.is_key_frame()) {
      if (first_sap_time_ == kInvalidTime)
        first_sap_time_ = pts;
    }
  }
  return Status::OK;
}

Status Fragmenter::InitializeFragment(int64_t first_sample_dts) {
  fragment_initialized_ = true;
  fragment_finalized_ = false;

  // |first_sample_dts| is adjusted by the edit list offset. The offset should
  // be un-applied in |decode_time|, so when applying the Edit List, the result
  // dts is |first_sample_dts|.
  const int64_t dts_before_edit = first_sample_dts + edit_list_offset_;
  traf_->decode_time.decode_time = dts_before_edit;

  traf_->runs.clear();
  traf_->runs.resize(1);
  traf_->runs[0].flags = TrackFragmentRun::kDataOffsetPresentMask;
  traf_->auxiliary_size.sample_info_sizes.clear();
  traf_->auxiliary_offset.offsets.clear();
  traf_->sample_encryption.sample_encryption_entries.clear();
  traf_->sample_group_descriptions.clear();
  traf_->sample_to_groups.clear();
  traf_->header.sample_description_index = 1;  // 1-based.
  traf_->header.flags = TrackFragmentHeader::kDefaultBaseIsMoofMask |
                        TrackFragmentHeader::kSampleDescriptionIndexPresentMask;

  fragment_duration_ = 0;
  earliest_presentation_time_ = kInvalidTime;
  first_sap_time_ = kInvalidTime;
  data_.reset(new BufferWriter());
  key_frame_infos_.clear();
  return Status::OK;
}

Status Fragmenter::FinalizeFragment() {
  if (!fragment_initialized_)
    return Status::OK;

  if (stream_info_->is_encrypted()) {
    Status status = FinalizeFragmentForEncryption();
    if (!status.ok())
      return status;
  }

  // Optimize trun box.
  traf_->runs[0].sample_count =
      static_cast<uint32_t>(traf_->runs[0].sample_sizes.size());
  if (OptimizeSampleEntries(&traf_->runs[0].sample_durations,
                            &traf_->header.default_sample_duration)) {
    traf_->header.flags |=
        TrackFragmentHeader::kDefaultSampleDurationPresentMask;
  } else {
    traf_->runs[0].flags |= TrackFragmentRun::kSampleDurationPresentMask;
  }
  if (OptimizeSampleEntries(&traf_->runs[0].sample_sizes,
                            &traf_->header.default_sample_size)) {
    traf_->header.flags |= TrackFragmentHeader::kDefaultSampleSizePresentMask;
  } else {
    traf_->runs[0].flags |= TrackFragmentRun::kSampleSizePresentMask;
  }
  if (OptimizeSampleEntries(&traf_->runs[0].sample_flags,
                            &traf_->header.default_sample_flags)) {
    traf_->header.flags |= TrackFragmentHeader::kDefaultSampleFlagsPresentMask;
  } else {
    traf_->runs[0].flags |= TrackFragmentRun::kSampleFlagsPresentMask;
  }

  // Add SampleToGroup boxes. A SampleToGroup box with grouping type of 'roll'
  // needs to be added if there is seek preroll, referencing sample group
  // description in track level; Also need to add SampleToGroup boxes
  // correponding to every SampleGroupDescription boxes, referencing sample
  // group description in fragment level.
  DCHECK_EQ(traf_->sample_to_groups.size(), 0u);
  if (seek_preroll_ > 0) {
    traf_->sample_to_groups.resize(traf_->sample_to_groups.size() + 1);
    SampleToGroup& sample_to_group = traf_->sample_to_groups.back();
    sample_to_group.grouping_type = FOURCC_roll;

    sample_to_group.entries.resize(1);
    SampleToGroupEntry& sample_to_group_entry = sample_to_group.entries.back();
    sample_to_group_entry.sample_count = traf_->runs[0].sample_count;
    sample_to_group_entry.group_description_index =
        SampleToGroupEntry::kTrackGroupDescriptionIndexBase + 1;
  }
  for (const auto& sample_group_description :
       traf_->sample_group_descriptions) {
    traf_->sample_to_groups.resize(traf_->sample_to_groups.size() + 1);
    SampleToGroup& sample_to_group = traf_->sample_to_groups.back();
    sample_to_group.grouping_type = sample_group_description.grouping_type;

    sample_to_group.entries.resize(1);
    SampleToGroupEntry& sample_to_group_entry = sample_to_group.entries.back();
    sample_to_group_entry.sample_count = traf_->runs[0].sample_count;
    sample_to_group_entry.group_description_index =
        SampleToGroupEntry::kTrackFragmentGroupDescriptionIndexBase + 1;
  }

  fragment_finalized_ = true;
  fragment_initialized_ = false;
  return Status::OK;
}

void Fragmenter::GenerateSegmentReference(SegmentReference* reference) const {
  // NOTE: Daisy chain is not supported currently.
  reference->reference_type = false;
  reference->subsegment_duration = fragment_duration_;
  reference->starts_with_sap = StartsWithSAP();
  if (kInvalidTime == first_sap_time_) {
    reference->sap_type = SegmentReference::TypeUnknown;
    reference->sap_delta_time = 0;
  } else {
    reference->sap_type = SegmentReference::Type1;
    reference->sap_delta_time = first_sap_time_ - earliest_presentation_time_;
  }
  reference->earliest_presentation_time = earliest_presentation_time_;
}

Status Fragmenter::FinalizeFragmentForEncryption() {
  SampleEncryption& sample_encryption = traf_->sample_encryption;
  if (sample_encryption.sample_encryption_entries.empty()) {
    // This fragment is not encrypted.
    // There are two sample description entries, an encrypted entry and a clear
    // entry, are generated. The 1-based clear entry index is always 2.
    const uint32_t kClearSampleDescriptionIndex = 2;
    traf_->header.sample_description_index = kClearSampleDescriptionIndex;
    return Status::OK;
  }
  if (sample_encryption.sample_encryption_entries.size() !=
      traf_->runs[0].sample_sizes.size()) {
    LOG(ERROR) << "Partially encrypted segment is not supported";
    return Status(error::MUXER_FAILURE,
                  "Partially encrypted segment is not supported.");
  }

  const SampleEncryptionEntry& sample_encryption_entry =
      sample_encryption.sample_encryption_entries.front();
  const bool use_subsample_encryption =
      !sample_encryption_entry.subsamples.empty();
  if (use_subsample_encryption)
    traf_->sample_encryption.flags |= SampleEncryption::kUseSubsampleEncryption;
  traf_->sample_encryption.iv_size = static_cast<uint8_t>(
      sample_encryption_entry.initialization_vector.size());

  // The offset will be adjusted in Segmenter after knowing moof size.
  traf_->auxiliary_offset.offsets.push_back(0);

  // Optimize saiz box.
  SampleAuxiliaryInformationSize& saiz = traf_->auxiliary_size;
  saiz.sample_count = static_cast<uint32_t>(saiz.sample_info_sizes.size());
  DCHECK_EQ(saiz.sample_info_sizes.size(),
            traf_->sample_encryption.sample_encryption_entries.size());
  if (!OptimizeSampleEntries(&saiz.sample_info_sizes,
                             &saiz.default_sample_info_size)) {
    saiz.default_sample_info_size = 0;
  }

  // It should only happen with full sample encryption + constant iv, i.e.
  // 'cbcs' applying to audio.
  if (saiz.default_sample_info_size == 0 && saiz.sample_info_sizes.empty()) {
    DCHECK(!use_subsample_encryption);
    // ISO/IEC 23001-7:2016(E) The sample auxiliary information would then be
    // empty and should be omitted. Clear saiz and saio boxes so they are not
    // written.
    saiz.sample_count = 0;
    traf_->auxiliary_offset.offsets.clear();
  }
  return Status::OK;
}

bool Fragmenter::StartsWithSAP() const {
  DCHECK(!traf_->runs.empty());
  uint32_t start_sample_flag;
  if (traf_->runs[0].flags & TrackFragmentRun::kSampleFlagsPresentMask) {
    DCHECK(!traf_->runs[0].sample_flags.empty());
    start_sample_flag = traf_->runs[0].sample_flags[0];
  } else {
    DCHECK(traf_->header.flags &
           TrackFragmentHeader::kDefaultSampleFlagsPresentMask);
    start_sample_flag = traf_->header.default_sample_flags;
  }
  return (start_sample_flag & TrackFragmentHeader::kNonKeySampleMask) == 0;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
