// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/mp4_fragmenter.h"

#include "media/base/aes_encryptor.h"
#include "media/base/buffer_reader.h"
#include "media/base/buffer_writer.h"
#include "media/base/media_sample.h"
#include "media/mp4/box_definitions.h"
#include "media/mp4/cenc.h"

namespace {
const uint64 kInvalidTime = kuint64max;

// Optimize sample entries table. If all values in |entries| are identical,
// then |entries| is cleared and the value is assigned to |default_value|;
// otherwise it is a NOP. Return true if the table is optimized.
template <typename T>
bool OptimizeSampleEntries(std::vector<T>* entries, T* default_value) {
  DCHECK(entries != NULL && default_value != NULL);
  DCHECK(!entries->empty());

  typename std::vector<T>::const_iterator it = entries->begin();
  T value = *it;
  for (; it < entries->end(); ++it)
    if (value != *it)
      return false;

  // Clear |entries| if it contains only one value.
  entries->clear();
  *default_value = value;
  return true;
}
}  // namespace

namespace media {
namespace mp4 {

MP4Fragmenter::MP4Fragmenter(TrackFragment* traf,
                             scoped_ptr<AesCtrEncryptor> encryptor,
                             int64 clear_time,
                             uint8 nalu_length_size)
    : encryptor_(encryptor.Pass()),
      nalu_length_size_(nalu_length_size),
      traf_(traf),
      fragment_finalized_(false),
      fragment_duration_(0),
      earliest_presentation_time_(0),
      first_sap_time_(0),
      clear_time_(clear_time) {}

MP4Fragmenter::~MP4Fragmenter() {}

Status MP4Fragmenter::AddSample(scoped_refptr<MediaSample> sample) {
  CHECK(sample->pts() >= sample->dts());
  CHECK(sample->duration() > 0);

  if (ShouldEncryptFragment()) {
    Status status = EncryptSample(sample);
    if (!status.ok())
      return status;
  }

  // Fill in sample parameters. It will be optimized later.
  traf_->runs[0].sample_sizes.push_back(sample->data_size());
  traf_->runs[0].sample_durations.push_back(sample->duration());
  traf_->runs[0].sample_flags.push_back(
      sample->is_key_frame() ? 0 : TrackFragmentHeader::kNonKeySampleMask);
  traf_->runs[0]
      .sample_composition_time_offsets.push_back(sample->pts() - sample->dts());
  if (sample->pts() != sample->dts()) {
    traf_->runs[0].flags |=
        TrackFragmentRun::kSampleCompTimeOffsetsPresentMask;
  }

  data_->AppendArray(sample->data(), sample->data_size());
  fragment_duration_ += sample->duration();

  if (earliest_presentation_time_ > sample->pts())
    earliest_presentation_time_ = sample->pts();

  if (sample->is_key_frame()) {
    if (kInvalidTime == first_sap_time_)
      first_sap_time_ = sample->pts();
  }
  return Status::OK;
}

void MP4Fragmenter::InitializeFragment() {
  fragment_finalized_ = false;
  traf_->decode_time.decode_time += fragment_duration_;
  traf_->auxiliary_size.sample_info_sizes.clear();
  traf_->auxiliary_offset.offsets.clear();
  traf_->runs.clear();
  traf_->runs.resize(1);
  traf_->runs[0].flags = TrackFragmentRun::kDataOffsetPresentMask;
  traf_->header.flags = TrackFragmentHeader::kDefaultBaseIsMoofMask;
  fragment_duration_ = 0;
  earliest_presentation_time_ = kInvalidTime;
  first_sap_time_ = kInvalidTime;
  data_.reset(new BufferWriter());
  aux_data_.reset(new BufferWriter());

  if (ShouldEncryptFragment()) {
    if (!IsSubsampleEncryptionRequired()) {
      DCHECK(encryptor_);
      traf_->auxiliary_size.default_sample_info_size = encryptor_->iv().size();
    }
  }
}

void MP4Fragmenter::FinalizeFragment() {
  if (ShouldEncryptFragment()) {
    DCHECK(encryptor_);

    // The offset will be adjusted in Segmenter when we know moof size.
    traf_->auxiliary_offset.offsets.push_back(0);

    // Optimize saiz box.
    SampleAuxiliaryInformationSize& saiz = traf_->auxiliary_size;
    saiz.sample_count = traf_->runs[0].sample_sizes.size();
    if (!saiz.sample_info_sizes.empty()) {
      if (!OptimizeSampleEntries(&saiz.sample_info_sizes,
                                 &saiz.default_sample_info_size)) {
        saiz.default_sample_info_size = 0;
      }
    }
  } else if (encryptor_ && clear_time_ > 0) {
    // This fragment should be in clear.
    // We generate at most two sample description entries, encrypted entry and
    // clear entry. The 1-based clear entry index is always 2.
    const uint32 kClearSampleDescriptionIndex = 2;

    traf_->header.flags |=
        TrackFragmentHeader::kSampleDescriptionIndexPresentMask;
    traf_->header.sample_description_index = kClearSampleDescriptionIndex;
    clear_time_ -= fragment_duration_;
  }

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
}

void MP4Fragmenter::GenerateSegmentReference(SegmentReference* reference) {
  // TODO(kqyang): support daisy chain??
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

void MP4Fragmenter::EncryptBytes(uint8* data, uint32 size) {
  DCHECK(encryptor_);
  CHECK(encryptor_->Encrypt(data, size, data));
}

Status MP4Fragmenter::EncryptSample(scoped_refptr<MediaSample> sample) {
  DCHECK(encryptor_);

  FrameCENCInfo cenc_info(encryptor_->iv());
  uint8* data = sample->writable_data();
  if (!IsSubsampleEncryptionRequired()) {
    EncryptBytes(data, sample->data_size());
  } else {
    BufferReader reader(data, sample->data_size());
    while (reader.HasBytes(1)) {
      uint64 nalu_length;
      if (!reader.ReadNBytesInto8(&nalu_length, nalu_length_size_))
        return Status(error::MUXER_FAILURE, "Fail to read nalu_length.");

      SubsampleEntry subsample;
      subsample.clear_bytes = nalu_length_size_ + 1;
      subsample.cypher_bytes = nalu_length - 1;
      if (!reader.SkipBytes(nalu_length)) {
        return Status(error::MUXER_FAILURE,
                      "Sample size does not match nalu_length.");
      }

      EncryptBytes(data + subsample.clear_bytes, subsample.cypher_bytes);
      cenc_info.AddSubsample(subsample);
      data += nalu_length_size_ + nalu_length;
    }

    // The length of per-sample auxiliary datum, defined in CENC ch. 7.
    traf_->auxiliary_size.sample_info_sizes.push_back(cenc_info.ComputeSize());
  }

  cenc_info.Write(aux_data_.get());
  encryptor_->UpdateIv();
  return Status::OK;
}

bool MP4Fragmenter::StartsWithSAP() {
  DCHECK(!traf_->runs.empty());
  uint32 start_sample_flag;
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
