// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/encrypting_fragmenter.h"

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/buffer_reader.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/filters/vp8_parser.h"
#include "packager/media/filters/vp9_parser.h"
#include "packager/media/formats/mp4/box_definitions.h"

namespace {
// Generate 64bit IV by default.
const size_t kDefaultIvSize = 8u;
}  // namespace

namespace edash_packager {
namespace media {
namespace mp4 {

EncryptingFragmenter::EncryptingFragmenter(
    TrackFragment* traf,
    scoped_ptr<EncryptionKey> encryption_key,
    int64_t clear_time,
    VideoCodec video_codec,
    uint8_t nalu_length_size)
    : Fragmenter(traf),
      encryption_key_(encryption_key.Pass()),
      video_codec_(video_codec),
      nalu_length_size_(nalu_length_size),
      clear_time_(clear_time) {
  DCHECK(encryption_key_);
  if (video_codec == kCodecVP8) {
    vpx_parser_.reset(new VP8Parser);
  } else if (video_codec == kCodecVP9) {
    vpx_parser_.reset(new VP9Parser);
  }
}

EncryptingFragmenter::~EncryptingFragmenter() {}

Status EncryptingFragmenter::AddSample(scoped_refptr<MediaSample> sample) {
  DCHECK(sample);
  if (!fragment_initialized()) {
    Status status = InitializeFragment(sample->dts());
    if (!status.ok())
      return status;
  }
  if (encryptor_) {
    Status status = EncryptSample(sample);
    if (!status.ok())
      return status;
  }
  return Fragmenter::AddSample(sample);
}

Status EncryptingFragmenter::InitializeFragment(int64_t first_sample_dts) {
  Status status = Fragmenter::InitializeFragment(first_sample_dts);
  if (!status.ok())
    return status;

  traf()->auxiliary_size.sample_info_sizes.clear();
  traf()->auxiliary_offset.offsets.clear();
  if (IsSubsampleEncryptionRequired()) {
    traf()->sample_encryption.flags |=
        SampleEncryption::kUseSubsampleEncryption;
  }
  traf()->sample_encryption.sample_encryption_entries.clear();

  const bool enable_encryption = clear_time_ <= 0;
  if (!enable_encryption) {
    // This fragment should be in clear text.
    // At most two sample description entries, an encrypted entry and a clear
    // entry, are generated. The 1-based clear entry index is always 2.
    const uint32_t kClearSampleDescriptionIndex = 2;

    traf()->header.flags |=
        TrackFragmentHeader::kSampleDescriptionIndexPresentMask;
    traf()->header.sample_description_index = kClearSampleDescriptionIndex;
  }
  return PrepareFragmentForEncryption(enable_encryption);
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

Status EncryptingFragmenter::PrepareFragmentForEncryption(
    bool enable_encryption) {
  return (!enable_encryption || encryptor_) ? Status::OK : CreateEncryptor();
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
  traf()->sample_encryption.iv_size = encryptor_->iv().size();
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

void EncryptingFragmenter::EncryptBytes(uint8_t* data, uint32_t size) {
  DCHECK(encryptor_);
  CHECK(encryptor_->Encrypt(data, size, data));
}

Status EncryptingFragmenter::EncryptSample(scoped_refptr<MediaSample> sample) {
  DCHECK(encryptor_);

  SampleEncryptionEntry sample_encryption_entry;
  sample_encryption_entry.initialization_vector = encryptor_->iv();
  uint8_t* data = sample->writable_data();
  if (IsSubsampleEncryptionRequired()) {
    if (vpx_parser_) {
      std::vector<VPxFrameInfo> vpx_frames;
      if (!vpx_parser_->Parse(sample->data(), sample->data_size(),
                              &vpx_frames)) {
        return Status(error::MUXER_FAILURE, "Failed to parse vpx frame.");
      }
      for (const VPxFrameInfo& frame : vpx_frames) {
        SubsampleEntry subsample;
        subsample.clear_bytes = frame.uncompressed_header_size;
        subsample.cipher_bytes =
            frame.frame_size - frame.uncompressed_header_size;
        sample_encryption_entry.subsamples.push_back(subsample);
        if (subsample.cipher_bytes > 0)
          EncryptBytes(data + subsample.clear_bytes, subsample.cipher_bytes);
        data += frame.frame_size;
      }
    } else {
      BufferReader reader(data, sample->data_size());
      while (reader.HasBytes(1)) {
        uint64_t nalu_length;
        if (!reader.ReadNBytesInto8(&nalu_length, nalu_length_size_))
          return Status(error::MUXER_FAILURE, "Fail to read nalu_length.");

        if (!reader.SkipBytes(nalu_length)) {
          return Status(error::MUXER_FAILURE,
                        "Sample size does not match nalu_length.");
        }

        SubsampleEntry subsample;
        subsample.clear_bytes = nalu_length_size_ + 1;
        subsample.cipher_bytes = nalu_length - 1;
        sample_encryption_entry.subsamples.push_back(subsample);

        EncryptBytes(data + subsample.clear_bytes, subsample.cipher_bytes);
        data += nalu_length_size_ + nalu_length;
      }
    }

    // The length of per-sample auxiliary datum, defined in CENC ch. 7.
    traf()->auxiliary_size.sample_info_sizes.push_back(
        sample_encryption_entry.ComputeSize());
  } else {
    EncryptBytes(data, sample->data_size());
  }

  traf()->sample_encryption.sample_encryption_entries.push_back(
      sample_encryption_entry);
  encryptor_->UpdateIv();
  return Status::OK;
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager
