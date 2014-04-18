// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/formats/mp4/key_rotation_fragmenter.h"

#include "media/base/aes_encryptor.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {
namespace mp4 {

KeyRotationFragmenter::KeyRotationFragmenter(
    MovieFragment* moof,
    TrackFragment* traf,
    bool normalize_presentation_timestamp,
    EncryptionKeySource* encryption_key_source,
    EncryptionKeySource::TrackType track_type,
    int64 crypto_period_duration,
    int64 clear_time,
    uint8 nalu_length_size)
    : Fragmenter(traf,
                 normalize_presentation_timestamp,
                 scoped_ptr<EncryptionKey>(new EncryptionKey()),
                 clear_time,
                 nalu_length_size),
      moof_(moof),
      encryption_key_source_(encryption_key_source),
      track_type_(track_type),
      crypto_period_duration_(crypto_period_duration),
      prev_crypto_period_index_(-1) {
  DCHECK(moof);
  DCHECK(encryption_key_source);
}

KeyRotationFragmenter::~KeyRotationFragmenter() {}

Status KeyRotationFragmenter::PrepareFragmentForEncryption() {
  traf()->auxiliary_size.sample_info_sizes.clear();
  traf()->auxiliary_offset.offsets.clear();

  size_t current_crypto_period_index =
      traf()->decode_time.decode_time / crypto_period_duration_;
  if (current_crypto_period_index != prev_crypto_period_index_) {
    scoped_ptr<EncryptionKey> encryption_key(new EncryptionKey());
    Status status = encryption_key_source_->GetCryptoPeriodKey(
        current_crypto_period_index, track_type_, encryption_key.get());
    if (!status.ok())
      return status;
    set_encryption_key(encryption_key.Pass());

    status = CreateEncryptor();
    if (!status.ok())
      return status;
    prev_crypto_period_index_ = current_crypto_period_index;
  }

  EncryptionKey* encryption_key = Fragmenter::encryption_key();
  DCHECK(encryption_key);
  AesCtrEncryptor* encryptor = Fragmenter::encryptor();
  DCHECK(encryptor);

  // We support key rotation in fragment boundary only, i.e. there is at most
  // one key for a single fragment. So we should have only one entry in
  // Sample Group Description box and one entry in Sample to Group box.
  // Fill in Sample Group Description box information.
  traf()->sample_group_description.grouping_type = FOURCC_SEIG;
  traf()->sample_group_description.entries.resize(1);
  traf()->sample_group_description.entries[0].is_encrypted = true;
  traf()->sample_group_description.entries[0].iv_size = encryptor->iv().size();
  traf()->sample_group_description.entries[0].key_id = encryption_key->key_id;

  // Fill in Sample to Group box information.
  traf()->sample_to_group.grouping_type = FOURCC_SEIG;
  traf()->sample_to_group.entries.resize(1);
  // sample_count is adjusted in |FinalizeFragment| later.
  traf()->sample_to_group.entries[0].group_description_index =
      SampleToGroupEntry::kTrackFragmentGroupDescriptionIndexBase + 1;

  // We need one and only one pssh box.
  if (moof_->pssh.empty())
    moof_->pssh.resize(1);
  moof_->pssh[0].raw_box = encryption_key->pssh;

  return Status::OK;
}

void KeyRotationFragmenter::FinalizeFragmentForEncryption() {
  Fragmenter::FinalizeFragmentForEncryption();
  DCHECK_EQ(1u, traf()->sample_to_group.entries.size());
  traf()->sample_to_group.entries[0].sample_count =
      traf()->auxiliary_size.sample_count;
}

}  // namespace media
}  // namespace mp4


