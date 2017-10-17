// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for raw key encryption.

#ifndef PACKAGER_APP_RAW_KEY_ENCRYPTION_FLAGS_H_
#define PACKAGER_APP_RAW_KEY_ENCRYPTION_FLAGS_H_

#include <gflags/gflags.h>

#include "packager/app/gflags_hex_bytes.h"

DECLARE_bool(enable_raw_key_encryption);
DECLARE_bool(enable_raw_key_decryption);
DECLARE_hex_bytes(key_id);
DECLARE_hex_bytes(key);
DECLARE_string(keys);
DECLARE_hex_bytes(iv);
DECLARE_hex_bytes(pssh);

namespace shaka {

/// Validate raw encryption/decryption flags.
/// @return true on success, false otherwise.
bool ValidateRawKeyCryptoFlags();

}  // namespace shaka

#endif  // PACKAGER_APP_RAW_KEY_ENCRYPTION_FLAGS_H_
