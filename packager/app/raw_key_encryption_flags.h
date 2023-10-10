// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for raw key encryption.

#ifndef PACKAGER_APP_RAW_KEY_ENCRYPTION_FLAGS_H_
#define PACKAGER_APP_RAW_KEY_ENCRYPTION_FLAGS_H_

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

#include <packager/utils/absl_flag_hexbytes.h>

ABSL_DECLARE_FLAG(bool, enable_raw_key_encryption);
ABSL_DECLARE_FLAG(bool, enable_raw_key_decryption);
ABSL_DECLARE_FLAG(shaka::HexBytes, key_id);
ABSL_DECLARE_FLAG(shaka::HexBytes, key);
ABSL_DECLARE_FLAG(std::string, keys);
ABSL_DECLARE_FLAG(shaka::HexBytes, iv);
ABSL_DECLARE_FLAG(shaka::HexBytes, pssh);

namespace shaka {

/// Validate raw encryption/decryption flags.
/// @return true on success, false otherwise.
bool ValidateRawKeyCryptoFlags();

}  // namespace shaka

#endif  // PACKAGER_APP_RAW_KEY_ENCRYPTION_FLAGS_H_
