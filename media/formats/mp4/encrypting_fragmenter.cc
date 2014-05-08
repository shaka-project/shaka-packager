// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/formats/mp4/encrypting_fragmenter.h"

#include "media/base/aes_encryptor.h"
#include "media/base/buffer_reader.h"
#include "media/base/encryption_key_source.h"
#include "media/base/media_sample.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/cenc.h"

namespace {
// Generate 64bit IV by default.
const size_t kDefaultIvSize = 8u;
}  // namespace

namespace media {
namespace mp4 {

EncryptingFragmenter::EncryptingFragmenter(
    TrackFragment* traf,
    bool normalize_presentation_timestamp,
    scoped_ptr<EncryptionKey> encryption_key,
    int64 clear_time,
    uint8 nalu_length_size)
    : Fragmenter(traf, normalize_presentation_timestamp),
      encryption_key_(encryption_key.Pass()),
      nalu_length_size_(nalu_length_size),
      clear_time_(clear_time) {
  DCHECK(encryption_key_);
}
EncryptingFragmenter::~EncryptingFragmenter() {}


Status EncryptingFragmenter::AddSample(scoped_refptr<MediaSample> sample) {
  DCHECK(sample);
  if (encryptor_) {
    Status status = EncryptSample(sample);
    if (!status.ok())
      return status;
  }
  return Fragmenter::AddSample(sample);
}

Status EncryptingFragmenter::InitializeFragment() {
  Status status = Fragmenter::InitializeFragment();
  if (!status.ok())
    return status;

  // Enable encryption for this fragment if |clear_time_| becomes non-positive.
  if (clear_time_ <= 0)
    return PrepareFragmentForEncryption();

  // Otherwise, this fragment should be in clear text.
  // At most two sample description entries, an encrypted entry and a clear
  // entry, are generated. The 1-based clear entry index is always 2.
  const uint32 kClearSampleDescriptionIndex = 2;

  traf()->header.flags |=
      TrackFragmentHeader::kSampleDescriptionIndexPresentMask;
  traf()->header.sample_description_index = kClearSampleDescriptionIndex;

  return Status::OK;
}

void EncryptingFragmenter::FinalizeFragment() {
  if (encryptor_) {
    DCHECK_LE(clear_time_, 0);
    FinalizeFragmentForEncryption();
  } else {
    DCHECK_GT(clear_time_, 0);
    clear_time_ -= fragment_duration();
  }
  Fragmenter::FinalizeFragment();
}

Status EncryptingFragmenter::PrepareFragmentForEncryption() {
  traf()->auxiliary_size.sample_info_sizes.clear();
  traf()->auxiliary_offset.offsets.clear();
  return encryptor_ ? Status::OK : CreateEncryptor();
}

void EncryptingFragmenter::FinalizeFragmentForEncryption() {
  // The offset will be adjusted in Segmenter after knowing moof size.
  traf()->auxiliary_offset.offsets.push_back(0);

  // Optimize saiz box.
  SampleAuxiliaryInformationSize& saiz = traf()->auxiliary_size;
  saiz.sample_count = traf()->runs[0].sample_sizes.size();
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

Status EncryptingFragmenter::CreateEncryptor() {
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

void EncryptingFragmenter::EncryptBytes(uint8* data, uint32 size) {
  DCHECK(encryptor_);
  CHECK(encryptor_->Encrypt(data, size, data));
}

Status EncryptingFragmenter::EncryptSample(scoped_refptr<MediaSample> sample) {
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
    traf()->auxiliary_size.sample_info_sizes.push_back(cenc_info.ComputeSize());
  }

  cenc_info.Write(aux_data());
  encryptor_->UpdateIv();
  return Status::OK;
}

}  // namespace mp4
}  // namespace media
