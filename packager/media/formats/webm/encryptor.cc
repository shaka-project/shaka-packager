// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/encryptor.h"

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/media_sample.h"

namespace shaka {
namespace media {
namespace webm {
namespace {

Status CreateContentEncryption(mkvmuxer::Track* track, EncryptionKey* key) {
  if (!track->AddContentEncoding()) {
    return Status(error::INTERNAL_ERROR,
                  "Could not add ContentEncoding to track.");
  }

  mkvmuxer::ContentEncoding* const encoding =
      track->GetContentEncodingByIndex(0);
  if (!encoding) {
    return Status(error::INTERNAL_ERROR,
                  "Could not add ContentEncoding to track.");
  }

  mkvmuxer::ContentEncAESSettings* const aes = encoding->enc_aes_settings();
  if (!aes) {
    return Status(error::INTERNAL_ERROR,
                  "Error getting ContentEncAESSettings.");
  }
  if (aes->cipher_mode() != mkvmuxer::ContentEncAESSettings::kCTR) {
    return Status(error::INTERNAL_ERROR, "Cipher Mode is not CTR.");
  }

  if (!key->key_id.empty() &&
      !encoding->SetEncryptionID(
          reinterpret_cast<const uint8*>(key->key_id.data()),
          key->key_id.size())) {
    return Status(error::INTERNAL_ERROR, "Error setting encryption ID.");
  }
  return Status::OK;
}

}  // namespace

Encryptor::Encryptor() {}

Encryptor::~Encryptor() {}

Status Encryptor::Initialize(MuxerListener* muxer_listener,
                             KeySource::TrackType track_type,
                             KeySource* key_source) {
  DCHECK(key_source);
  return CreateEncryptor(muxer_listener, track_type, key_source);
}

Status Encryptor::AddTrackInfo(mkvmuxer::Track* track) {
  DCHECK(key_);
  return CreateContentEncryption(track, key_.get());
}

Status Encryptor::EncryptFrame(scoped_refptr<MediaSample> sample,
                               bool encrypt_frame) {
  DCHECK(encryptor_);

  const size_t sample_size = sample->data_size();
  if (encrypt_frame) {
    // | 1 | iv | enc_data |
    const size_t iv_size = encryptor_->iv().size();
    sample->resize_data(sample_size + iv_size + 1);
    uint8_t* sample_data = sample->writable_data();

    // Encrypt the data in-place.
    if (!encryptor_->Crypt(sample_data, sample_size, sample_data)) {
      return Status(error::MUXER_FAILURE, "Failed to encrypt the frame.");
    }

    // First move the sample data to after the IV; then write the IV and signal
    // byte.
    memmove(sample_data + iv_size + 1, sample_data, sample_size);
    sample_data[0] = 0x01;
    memcpy(sample_data + 1, encryptor_->iv().data(), iv_size);

    encryptor_->UpdateIv();
  } else {
    // | 0 | data |
    sample->resize_data(sample_size + 1);
    uint8_t* sample_data = sample->writable_data();
    memmove(sample_data + 1, sample_data, sample_size);
    sample_data[0] = 0x00;
  }

  return Status::OK;
}

Status Encryptor::CreateEncryptor(MuxerListener* muxer_listener,
                                  KeySource::TrackType track_type,
                                  KeySource* key_source) {
  scoped_ptr<EncryptionKey> encryption_key(new EncryptionKey());
  Status status = key_source->GetKey(track_type, encryption_key.get());
  if (!status.ok())
    return status;
  if (encryption_key->iv.empty()) {
    if (!AesCryptor::GenerateRandomIv(FOURCC_cenc, &encryption_key->iv))
      return Status(error::INTERNAL_ERROR, "Failed to generate random iv.");
  }

  scoped_ptr<AesCtrEncryptor> encryptor(new AesCtrEncryptor());
  const bool initialized =
      encryptor->InitializeWithIv(encryption_key->key, encryption_key->iv);
  if (!initialized)
    return Status(error::INTERNAL_ERROR, "Failed to create the encryptor.");

  if (muxer_listener) {
    const bool kInitialEncryptionInfo = true;
    muxer_listener->OnEncryptionInfoReady(
        kInitialEncryptionInfo, FOURCC_cenc, encryption_key->key_id,
        encryptor->iv(), encryption_key->key_system_info);
  }

  key_ = encryption_key.Pass();
  encryptor_ = encryptor.Pass();
  return Status::OK;
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
