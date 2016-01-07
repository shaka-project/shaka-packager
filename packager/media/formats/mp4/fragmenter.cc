// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/fragmenter.h"

#include <limits>

#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/formats/mp4/box_definitions.h"

namespace edash_packager {
namespace media {
namespace mp4 {

namespace {
const int64_t kInvalidTime = std::numeric_limits<int64_t>::max();
}  // namespace

Fragmenter::Fragmenter(TrackFragment* traf)
    : traf_(traf),
      fragment_initialized_(false),
      fragment_finalized_(false),
      fragment_duration_(0),
      presentation_start_time_(kInvalidTime),
      earliest_presentation_time_(kInvalidTime),
      first_sap_time_(kInvalidTime) {
  DCHECK(traf);
}

Fragmenter::~Fragmenter() {}

Status Fragmenter::AddSample(scoped_refptr<MediaSample> sample) {
  DCHECK(sample);
  CHECK_GT(sample->duration(), 0);

  if (!fragment_initialized_) {
    Status status = InitializeFragment(sample->dts());
    if (!status.ok())
      return status;
  }

  if (sample->side_data_size() > 0)
    LOG(WARNING) << "MP4 samples do not support side data. Side data ignored.";

  // Fill in sample parameters. It will be optimized later.
  traf_->runs[0].sample_sizes.push_back(sample->data_size());
  traf_->runs[0].sample_durations.push_back(sample->duration());
  traf_->runs[0].sample_flags.push_back(
      sample->is_key_frame() ? 0 : TrackFragmentHeader::kNonKeySampleMask);

  data_->AppendArray(sample->data(), sample->data_size());
  fragment_duration_ += sample->duration();

  int64_t pts = sample->pts();

  // Set |earliest_presentation_time_| to |pts| if |pts| is smaller or if it is
  // not yet initialized (kInvalidTime > pts is always true).
  if (earliest_presentation_time_ > pts)
    earliest_presentation_time_ = pts;

  traf_->runs[0].sample_composition_time_offsets.push_back(pts - sample->dts());
  if (pts != sample->dts())
    traf_->runs[0].flags |= TrackFragmentRun::kSampleCompTimeOffsetsPresentMask;

  if (sample->is_key_frame()) {
    if (first_sap_time_ == kInvalidTime)
      first_sap_time_ = pts;
  }
  return Status::OK;
}

Status Fragmenter::InitializeFragment(int64_t first_sample_dts) {
  fragment_initialized_ = true;
  fragment_finalized_ = false;
  traf_->decode_time.decode_time = first_sample_dts;
  traf_->runs.clear();
  traf_->runs.resize(1);
  traf_->runs[0].flags = TrackFragmentRun::kDataOffsetPresentMask;
  traf_->header.sample_description_index = 1;  // 1-based.
  traf_->header.flags = TrackFragmentHeader::kDefaultBaseIsMoofMask |
                        TrackFragmentHeader::kSampleDescriptionIndexPresentMask;
  fragment_duration_ = 0;
  earliest_presentation_time_ = kInvalidTime;
  first_sap_time_ = kInvalidTime;
  data_.reset(new BufferWriter());
  return Status::OK;
}

void Fragmenter::FinalizeFragment() {
  // Optimize trun box.
  traf_->runs[0].sample_count = traf_->runs[0].sample_sizes.size();
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

  fragment_finalized_ = true;
  fragment_initialized_ = false;
}

void Fragmenter::GenerateSegmentReference(SegmentReference* reference) {
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

bool Fragmenter::StartsWithSAP() {
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
}  // namespace edash_packager
