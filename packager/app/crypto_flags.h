// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines common command line flags for encryption and decryption, which
// applies to all key sources, i.e. raw key, Widevine and PlayReady.

#ifndef PACKAGER_APP_CRYPTO_FLAGS_H_
#define PACKAGER_APP_CRYPTO_FLAGS_H_

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

ABSL_DECLARE_FLAG(std::string, protection_scheme);
ABSL_DECLARE_FLAG(int32_t, crypt_byte_block);
ABSL_DECLARE_FLAG(int32_t, skip_byte_block);
ABSL_DECLARE_FLAG(bool, vp9_subsample_encryption);
ABSL_DECLARE_FLAG(std::string, playready_extra_header_data);

namespace shaka {
bool ValidateCryptoFlags();
}

#endif  // PACKAGER_APP_CRYPTO_FLAGS_H_
