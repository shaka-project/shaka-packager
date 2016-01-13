// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/decryptor_source.h"

#include "packager/base/logging.h"
#include "packager/base/stl_util.h"

namespace edash_packager {
namespace media {

DecryptorSource::DecryptorSource(KeySource* key_source)
    : key_source_(key_source) {
  CHECK(key_source);
}
DecryptorSource::~DecryptorSource() {
  STLDeleteValues(&decryptor_map_);
}

bool DecryptorSource::DecryptSampleBuffer(const DecryptConfig* decrypt_config,
                                          uint8_t* buffer,
                                          size_t buffer_size) {
  DCHECK(decrypt_config);
  DCHECK(buffer);

  // Get the decryptor object.
  AesCtrEncryptor* decryptor;
  auto found = decryptor_map_.find(decrypt_config->key_id());
  if (found == decryptor_map_.end()) {
    // Create new AesCtrEncryptor
    EncryptionKey key;
    Status status(key_source_->GetKey(decrypt_config->key_id(), &key));
    if (!status.ok()) {
      LOG(ERROR) << "Error retrieving decryption key: " << status;
      return false;
    }
    scoped_ptr<AesCtrEncryptor> aes_ctr_encryptor(new AesCtrEncryptor);
    if (!aes_ctr_encryptor->InitializeWithIv(key.key, decrypt_config->iv())) {
      LOG(ERROR) << "Failed to initialize AesCtrEncryptor for decryption.";
      return false;
    }
    decryptor = aes_ctr_encryptor.release();
    decryptor_map_[decrypt_config->key_id()] = decryptor;
  } else {
    decryptor = found->second;
  }
  if (!decryptor->SetIv(decrypt_config->iv())) {
    LOG(ERROR) << "Invalid initialization vector.";
    return false;
  }

  if (decrypt_config->subsamples().empty()) {
    // Sample not encrypted using subsample encryption. Decrypt whole.
    if (!decryptor->Decrypt(buffer, buffer_size, buffer)) {
      LOG(ERROR) << "Error during bulk sample decryption.";
      return false;
    }
    return true;
  }

  // Subsample decryption.
  const std::vector<SubsampleEntry>& subsamples = decrypt_config->subsamples();
  uint8_t* current_ptr = buffer;
  const uint8_t* const buffer_end = buffer + buffer_size;
  for (const auto& subsample : subsamples) {
    if ((current_ptr + subsample.clear_bytes + subsample.cipher_bytes) >
        buffer_end) {
      LOG(ERROR) << "Subsamples overflow sample buffer.";
      return false;
    }
    current_ptr += subsample.clear_bytes;
    if (!decryptor->Decrypt(current_ptr, subsample.cipher_bytes, current_ptr)) {
      LOG(ERROR) << "Error decrypting subsample buffer.";
      return false;
    }
    current_ptr += subsample.cipher_bytes;
  }
  return true;
}

}  // namespace media
}  // namespace edash_packager
