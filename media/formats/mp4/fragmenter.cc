// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/formats/mp4/fragmenter.h"

#include "media/base/aes_encryptor.h"
#include "media/base/buffer_reader.h"
#include "media/base/buffer_writer.h"
#include "media/base/encryption_key_source.h"
#include "media/base/media_sample.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/cenc.h"

namespace media {
namespace mp4 {

namespace {
// Generate 64bit IV by default.
const size_t kDefaultIvSize = 8u;
const int64 kInvalidTime = kint64max;

// Optimize sample entries table. If all values in |entries| are identical,
// then |entries| is cleared and the value is assigned to |default_value|;
// otherwise it is a NOP. Return true if the table is optimized.
template <typename T>
bool OptimizeSampleEntries(std::vector<T>* entries, T* default_value) {
  DCHECK(entries);
  DCHECK(default_value);
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

Fragmenter::Fragmenter(TrackFragment* traf,
                       bool normalize_presentation_timestamp)
    : traf_(traf),
      nalu_length_size_(0),
      clear_time_(0),
      fragment_finalized_(false),
      fragment_duration_(0),
      normalize_presentation_timestamp_(normalize_presentation_timestamp),
      presentation_start_time_(kInvalidTime),
      earliest_presentation_time_(kInvalidTime),
      first_sap_time_(kInvalidTime) {
  DCHECK(traf);
}

Fragmenter::Fragmenter(TrackFragment* traf,
                       bool normalize_presentation_timestamp,
                       scoped_ptr<EncryptionKey> encryption_key,
                       int64 clear_time,
                       uint8 nalu_length_size)
    : traf_(traf),
      encryption_key_(encryption_key.Pass()),
      nalu_length_size_(nalu_length_size),
      clear_time_(clear_time),
      fragment_finalized_(false),
      fragment_duration_(0),
      normalize_presentation_timestamp_(normalize_presentation_timestamp),
      presentation_start_time_(kInvalidTime),
      earliest_presentation_time_(kInvalidTime),
      first_sap_time_(kInvalidTime) {
  DCHECK(traf);
  DCHECK(encryption_key_);
}

Fragmenter::~Fragmenter() {}

Status Fragmenter::AddSample(scoped_refptr<MediaSample> sample) {
  CHECK_GT(sample->duration(), 0);

  if (encryptor_) {
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

  int64 pts = sample->pts();
  if (normalize_presentation_timestamp_) {
    // Normalize PTS to start from 0. Some players do not like non-zero
    // presentation starting time.
    // NOTE: The timeline of the remuxed video may not be exactly the same as
    // the original video. An EditList box may be useful to solve this.
    if (presentation_start_time_ == kInvalidTime) {
      presentation_start_time_ = pts;
      pts = 0;
    } else {
      // Can we safely assume the first sample in the media has the earliest
      // presentation timestamp?
      DCHECK_GT(pts, presentation_start_time_);
      pts -= presentation_start_time_;
    }
  }

  // Set |earliest_presentation_time_| to |pts| if |pts| is smaller or if it is
  // not yet initialized (kInvalidTime > pts is always true).
  if (earliest_presentation_time_ > pts)
    earliest_presentation_time_ = pts;

  if (sample->is_key_frame()) {
    if (first_sap_time_ == kInvalidTime)
      first_sap_time_ = pts;
  }
  return Status::OK;
}

Status Fragmenter::InitializeFragment() {
  fragment_finalized_ = false;
  traf_->decode_time.decode_time += fragment_duration_;
  traf_->runs.clear();
  traf_->runs.resize(1);
  traf_->runs[0].flags = TrackFragmentRun::kDataOffsetPresentMask;
  traf_->header.flags = TrackFragmentHeader::kDefaultBaseIsMoofMask;
  fragment_duration_ = 0;
  earliest_presentation_time_ = kInvalidTime;
  first_sap_time_ = kInvalidTime;
  data_.reset(new BufferWriter());
  aux_data_.reset(new BufferWriter());

  if (!encryption_key_)
    return Status::OK;

  // Enable encryption for this fragment if decode time passes clear lead.
  if (static_cast<int64>(traf_->decode_time.decode_time) >= clear_time_)
    return PrepareFragmentForEncryption();

  // Otherwise, this fragment should be in clear text.
  // We generate at most two sample description entries, encrypted entry and
  // clear entry. The 1-based clear entry index is always 2.
  const uint32 kClearSampleDescriptionIndex = 2;

  traf_->header.flags |=
      TrackFragmentHeader::kSampleDescriptionIndexPresentMask;
  traf_->header.sample_description_index = kClearSampleDescriptionIndex;

  return Status::OK;
}

void Fragmenter::FinalizeFragment() {
  if (encryptor_)
    FinalizeFragmentForEncryption();

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

Status Fragmenter::PrepareFragmentForEncryption() {
  traf_->auxiliary_size.sample_info_sizes.clear();
  traf_->auxiliary_offset.offsets.clear();
  return encryptor_ ? Status::OK : CreateEncryptor();
}

void Fragmenter::FinalizeFragmentForEncryption() {
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
  } else {
    // |sample_info_sizes| table is filled in only for subsample encryption,
    // otherwise |sample_info_size| is just the IV size.
    DCHECK(!IsSubsampleEncryptionRequired());
    saiz.default_sample_info_size = encryptor_->iv().size();
  }
}

Status Fragmenter::CreateEncryptor() {
  DCHECK(encryption_key_);

  scoped_ptr<AesCtrEncryptor> encryptor(new AesCtrEncryptor());
  const bool initialized = encryption_key_->iv.empty()
                               ? encryptor->InitializeWithRandomIv(
                                     encryption_key_->key, kDefaultIvSize)
                               : encryptor->InitializeWithIv(
                                     encryption_key_->key, encryption_key_->iv);
  if (!initialized)
    return Status(error::MUXER_FAILURE, "Failed to create the encryptor.");
  encryptor_ = encryptor.Pass();
  return Status::OK;
}

void Fragmenter::EncryptBytes(uint8* data, uint32 size) {
  DCHECK(encryptor_);
  CHECK(encryptor_->Encrypt(data, size, data));
}

Status Fragmenter::EncryptSample(scoped_refptr<MediaSample> sample) {
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

bool Fragmenter::StartsWithSAP() {
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
