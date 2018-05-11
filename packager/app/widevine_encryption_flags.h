// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for widevine_encryption.

#ifndef APP_WIDEVINE_ENCRYPTION_FLAGS_H_
#define APP_WIDEVINE_ENCRYPTION_FLAGS_H_

#include <gflags/gflags.h>

#include "packager/app/gflags_hex_bytes.h"

DECLARE_bool(enable_widevine_encryption);
DECLARE_bool(enable_widevine_decryption);
DECLARE_string(key_server_url);
DECLARE_hex_bytes(content_id);
DECLARE_string(policy);
DECLARE_int32(max_sd_pixels);
DECLARE_int32(max_hd_pixels);
DECLARE_int32(max_uhd1_pixels);
DECLARE_string(signer);
DECLARE_hex_bytes(aes_signing_key);
DECLARE_hex_bytes(aes_signing_iv);
DECLARE_string(rsa_signing_key_path);
DECLARE_int32(crypto_period_duration);
DECLARE_hex_bytes(group_id);
DECLARE_bool(enable_entitlement_license);

namespace shaka {

/// Validate widevine encryption/decryption flags.
/// @return true on success, false otherwise.
bool ValidateWidevineCryptoFlags();

}  // namespace shaka

#endif  // APP_WIDEVINE_ENCRYPTION_FLAGS_H_
