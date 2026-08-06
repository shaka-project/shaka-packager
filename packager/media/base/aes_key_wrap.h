// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_AES_KEY_WRAP_H_
#define PACKAGER_MEDIA_BASE_AES_KEY_WRAP_H_

#include <cstdint>
#include <vector>

namespace shaka {
namespace media {

/// AES Key Unwrap (RFC 3394 / NIST SP 800-38F KW mode). Fails if the
/// built-in integrity check does not pass, i.e. if @a wrapped_data was not
/// produced with @a wrapping_key or was modified.
/// @param wrapping_key is the key-encryption key; 16, 24 or 32 bytes.
/// @param wrapped_data is the wrapped key data; a multiple of 8 bytes, at
///        least 24.
/// @param data receives the unwrapped output (8 bytes shorter than
///        @a wrapped_data). Should not be NULL.
/// @return true on success, false otherwise.
bool AesKeyUnwrap(const std::vector<uint8_t>& wrapping_key,
                  const std::vector<uint8_t>& wrapped_data,
                  std::vector<uint8_t>* data);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_AES_KEY_WRAP_H_
