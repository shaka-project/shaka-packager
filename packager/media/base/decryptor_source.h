// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_DECRYPTOR_SOURCE_H_
#define PACKAGER_MEDIA_BASE_DECRYPTOR_SOURCE_H_

#include <map>
#include <memory>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/aes_decryptor.h>
#include <packager/media/base/decrypt_config.h>
#include <packager/media/base/key_source.h>

namespace shaka {
namespace media {

/// DecryptorSource wraps KeySource and is responsible for decryptor management.
class DecryptorSource {
 public:
  /// Constructs a DecryptorSource object.
  /// @param key_source points to the key source that contains the keys.
  explicit DecryptorSource(KeySource* key_source);
  ~DecryptorSource();

  /// Decrypt encrypted buffer.
  /// @param decrypt_config contains decrypt configuration, e.g. protection
  ///        scheme, subsample information etc.
  /// @param encrypted_buffer points to the encrypted buffer that is to be
  ///        decrypted. It should not overlap with @a decrypted_buffer.
  /// @param buffer_size is the size of encrypted buffer and decrypted buffer.
  /// @param decrypted_buffer points to the decrypted buffer. It should not
  ///        overlap with @a encrypted_buffer.
  /// @return true if success, false otherwise.
  bool DecryptSampleBuffer(const DecryptConfig* decrypt_config,
                           const uint8_t* encrypted_buffer,
                           size_t buffer_size,
                           uint8_t* decrypted_buffer);

 private:
  KeySource* key_source_;
  std::map<std::vector<uint8_t>, std::unique_ptr<AesCryptor>> decryptor_map_;

  DISALLOW_COPY_AND_ASSIGN(DecryptorSource);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_DECRYPTOR_SOURCE_H_
