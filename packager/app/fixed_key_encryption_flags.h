// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for fixed key encryption.

#ifndef APP_FIXED_KEY_ENCRYPTION_FLAGS_H_
#define APP_FIXED_KEY_ENCRYPTION_FLAGS_H_

#include <gflags/gflags.h>

#include "packager/app/gflags_hex_bytes.h"

// TODO(kqyang): s/fixed/raw/.
DECLARE_bool(enable_fixed_key_encryption);
DECLARE_bool(enable_fixed_key_decryption);
DECLARE_hex_bytes(key_id);
DECLARE_hex_bytes(key);
DECLARE_hex_bytes(iv);
DECLARE_hex_bytes(pssh);

namespace shaka {

/// Validate fixed encryption/decryption flags.
/// @return true on success, false otherwise.
bool ValidateFixedCryptoFlags();

}  // namespace shaka

#endif  // APP_FIXED_KEY_ENCRYPTION_FLAGS_H_
