// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CRYPTO_HELPERS_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CRYPTO_HELPERS_H_

#include <stdint.h>
#include <memory>
#include "packager/media/base/decrypt_config.h"

namespace shaka {
namespace media {

/// Fills an initialized DecryptConfig, which can be sent to the Decryptor if
/// the stream has potentially encrypted frames. Also sets |data_offset| which
/// indicates where the encrypted data starts. Leaving the IV empty will tell
/// the decryptor that the frame is unencrypted.
/// Current encrypted WebM request for comments specification is here
/// http://wiki.webmproject.org/encryption/webm-encryption-rfc
/// @return true if |data| is valid, false otherwise, in which case
///         |decrypt_config| and |data_offset| will not be changed.
bool WebMCreateDecryptConfig(const uint8_t* data,
                             int data_size,
                             const uint8_t* key_id,
                             size_t key_id_size,
                             std::unique_ptr<DecryptConfig>* decrypt_config,
                             int* data_offset);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CRYPT_HELPERS_H_
