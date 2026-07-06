// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/aes_key_wrap.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <mbedtls/nist_kw.h>

namespace shaka {
namespace media {
namespace {

bool IsValidWrappingKeySize(size_t size) {
  return size == 16 || size == 24 || size == 32;
}

}  // namespace

bool AesKeyUnwrap(const std::vector<uint8_t>& wrapping_key,
                  const std::vector<uint8_t>& wrapped_data,
                  std::vector<uint8_t>* data) {
  DCHECK(data);
  if (!IsValidWrappingKeySize(wrapping_key.size())) {
    LOG(ERROR) << "Invalid AES key wrap key size: " << wrapping_key.size();
    return false;
  }

  mbedtls_nist_kw_context context;
  mbedtls_nist_kw_init(&context);
  int rv = mbedtls_nist_kw_setkey(
      &context, MBEDTLS_CIPHER_ID_AES, wrapping_key.data(),
      static_cast<unsigned>(wrapping_key.size()) * 8,
      /* is_wrap= */ 0);
  if (rv != 0) {
    LOG(ERROR) << "AES key unwrap setkey failed: " << rv;
    mbedtls_nist_kw_free(&context);
    return false;
  }

  data->resize(wrapped_data.size());
  size_t output_size = 0;
  rv = mbedtls_nist_kw_unwrap(&context, MBEDTLS_KW_MODE_KW, wrapped_data.data(),
                              wrapped_data.size(), data->data(), &output_size,
                              data->size());
  mbedtls_nist_kw_free(&context);
  if (rv != 0) {
    LOG(ERROR) << "AES key unwrap failed: " << rv;
    return false;
  }
  data->resize(output_size);
  return true;
}

}  // namespace media
}  // namespace shaka
