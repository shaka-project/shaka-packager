// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/formats/mp4/key_rotation_fragmenter.h"

#include "media/base/aes_encryptor.h"
#include "media/formats/mp4/box_definitions.h"

namespace edash_packager {
namespace media {
namespace mp4 {

KeyRotationFragmenter::KeyRotationFragmenter(
    MovieFragment* moof,
    TrackFragment* traf,
    KeySource* encryption_key_source,
    KeySource::TrackType track_type,
    int64 crypto_period_duration,
    int64 clear_time,
    uint8 nalu_length_size)
    : EncryptingFragmenter(traf,
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

Status KeyRotationFragmenter::PrepareFragmentForEncryption(
    bool enable_encryption) {
  bool need_to_refresh_encryptor = !encryptor();

  size_t current_crypto_period_index =
      traf()->decode_time.decode_time / crypto_period_duration_;
  if (current_crypto_period_index != prev_crypto_period_index_) {
    scoped_ptr<EncryptionKey> encryption_key(new EncryptionKey());
    Status status = encryption_key_source_->GetCryptoPeriodKey(
        current_crypto_period_index, track_type_, encryption_key.get());
    if (!status.ok())
      return status;
    set_encryption_key(encryption_key.Pass());
    prev_crypto_period_index_ = current_crypto_period_index;
    need_to_refresh_encryptor = true;
  }

  // One and only one 'pssh' box is needed.
  if (moof_->pssh.empty())
    moof_->pssh.resize(1);
  DCHECK(encryption_key());
  moof_->pssh[0].raw_box = encryption_key()->pssh;

  // Skip the following steps if the current fragment is not going to be
  // encrypted. 'pssh' box needs to be included in the fragment, which is
  // performed above, regardless of whether the fragment is encrypted. This is
  // necessary for two reasons: 1) Requesting keys before reaching encrypted
  // content avoids playback delay due to license requests; 2) In Chrome, CDM
  // must be initialized before starting the playback and CDM can only be
  // initialized with a valid 'pssh'.
  if (!enable_encryption) {
    DCHECK(!encryptor());
    return Status::OK;
  }

  if (need_to_refresh_encryptor) {
    Status status = CreateEncryptor();
    if (!status.ok())
      return status;
  }
  DCHECK(encryptor());

  // Key rotation happens in fragment boundary only in this implementation,
  // i.e. there is at most one key for the fragment. So there should be only
  // one entry in SampleGroupDescription box and one entry in SampleToGroup box.
  // Fill in SampleGroupDescription box information.
  traf()->sample_group_description.grouping_type = FOURCC_SEIG;
  traf()->sample_group_description.entries.resize(1);
  traf()->sample_group_description.entries[0].is_encrypted = true;
  traf()->sample_group_description.entries[0].iv_size =
      encryptor()->iv().size();
  traf()->sample_group_description.entries[0].key_id = encryption_key()->key_id;

  // Fill in SampleToGroup box information.
  traf()->sample_to_group.grouping_type = FOURCC_SEIG;
  traf()->sample_to_group.entries.resize(1);
  // sample_count is adjusted in |FinalizeFragment| later.
  traf()->sample_to_group.entries[0].group_description_index =
      SampleToGroupEntry::kTrackFragmentGroupDescriptionIndexBase + 1;

  return Status::OK;
}

void KeyRotationFragmenter::FinalizeFragmentForEncryption() {
  EncryptingFragmenter::FinalizeFragmentForEncryption();
  DCHECK_EQ(1u, traf()->sample_to_group.entries.size());
  traf()->sample_to_group.entries[0].sample_count =
      traf()->auxiliary_size.sample_count;
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager
