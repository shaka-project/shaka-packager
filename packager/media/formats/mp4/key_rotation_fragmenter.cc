// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/key_rotation_fragmenter.h"

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/formats/mp4/box_definitions.h"

namespace edash_packager {
namespace media {
namespace mp4 {

namespace {
const bool kInitialEncryptionInfo = true;
}  // namespace

KeyRotationFragmenter::KeyRotationFragmenter(MovieFragment* moof,
                                             scoped_refptr<StreamInfo> info,
                                             TrackFragment* traf,
                                             KeySource* encryption_key_source,
                                             KeySource::TrackType track_type,
                                             int64_t crypto_period_duration,
                                             int64_t clear_time,
                                             FourCC protection_scheme,
                                             uint8_t crypt_byte_block,
                                             uint8_t skip_byte_block,
                                             MuxerListener* muxer_listener)
    : EncryptingFragmenter(info,
                           traf,
                           scoped_ptr<EncryptionKey>(new EncryptionKey()),
                           clear_time,
                           protection_scheme,
                           crypt_byte_block,
                           skip_byte_block),
      moof_(moof),
      encryption_key_source_(encryption_key_source),
      track_type_(track_type),
      crypto_period_duration_(crypto_period_duration),
      prev_crypto_period_index_(-1),
      muxer_listener_(muxer_listener) {
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
    if (encryption_key->iv.empty()) {
      if (!AesCryptor::GenerateRandomIv(protection_scheme(),
                                        &encryption_key->iv)) {
        return Status(error::INTERNAL_ERROR, "Failed to generate random iv.");
      }
    }
    set_encryption_key(encryption_key.Pass());
    prev_crypto_period_index_ = current_crypto_period_index;
    need_to_refresh_encryptor = true;
  }

  DCHECK(encryption_key());
  const std::vector<ProtectionSystemSpecificInfo>& system_info =
      encryption_key()->key_system_info;
  moof_->pssh.resize(system_info.size());
  for (size_t i = 0; i < system_info.size(); i++) {
    moof_->pssh[i].raw_box = system_info[i].CreateBox();
  }

  if (muxer_listener_) {
    muxer_listener_->OnEncryptionInfoReady(
        !kInitialEncryptionInfo, protection_scheme(), encryption_key()->key_id,
        encryption_key()->iv, encryption_key()->key_system_info);
  }

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
  traf()->sample_group_descriptions.resize(
      traf()->sample_group_descriptions.size() + 1);
  SampleGroupDescription& sample_group_description =
      traf()->sample_group_descriptions.back();
  sample_group_description.grouping_type = FOURCC_seig;

  sample_group_description.cenc_sample_encryption_info_entries.resize(1);
  CencSampleEncryptionInfoEntry& sample_group_entry =
      sample_group_description.cenc_sample_encryption_info_entries.back();
  sample_group_entry.is_protected = 1;
  if (protection_scheme() == FOURCC_cbcs) {
    // For 'cbcs' scheme, Constant IVs SHALL be used.
    sample_group_entry.per_sample_iv_size = 0;
    sample_group_entry.constant_iv = encryptor()->iv();
  } else {
    sample_group_entry.per_sample_iv_size = encryptor()->iv().size();
  }
  sample_group_entry.crypt_byte_block = crypt_byte_block();
  sample_group_entry.skip_byte_block = skip_byte_block();
  sample_group_entry.key_id = encryption_key()->key_id;

  return Status::OK;
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager
