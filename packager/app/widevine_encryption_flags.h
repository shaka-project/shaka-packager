// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for widevine_encryption.

#ifndef APP_WIDEVINE_ENCRYPTION_FLAGS_H_
#define APP_WIDEVINE_ENCRYPTION_FLAGS_H_

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

#include <packager/utils/absl_flag_hexbytes.h>

ABSL_DECLARE_FLAG(bool, enable_widevine_encryption);
ABSL_DECLARE_FLAG(bool, enable_widevine_decryption);
ABSL_DECLARE_FLAG(std::string, key_server_url);
ABSL_DECLARE_FLAG(shaka::HexBytes, content_id);
ABSL_DECLARE_FLAG(std::string, policy);
ABSL_DECLARE_FLAG(int32_t, max_sd_pixels);
ABSL_DECLARE_FLAG(int32_t, max_hd_pixels);
ABSL_DECLARE_FLAG(int32_t, max_uhd1_pixels);
ABSL_DECLARE_FLAG(std::string, signer);
ABSL_DECLARE_FLAG(shaka::HexBytes, aes_signing_key);
ABSL_DECLARE_FLAG(shaka::HexBytes, aes_signing_iv);
ABSL_DECLARE_FLAG(std::string, rsa_signing_key_path);
ABSL_DECLARE_FLAG(int32_t, crypto_period_duration);
ABSL_DECLARE_FLAG(shaka::HexBytes, group_id);
ABSL_DECLARE_FLAG(bool, enable_entitlement_license);

namespace shaka {

/// Validate widevine encryption/decryption flags.
/// @return true on success, false otherwise.
bool ValidateWidevineCryptoFlags();

}  // namespace shaka

#endif  // APP_WIDEVINE_ENCRYPTION_FLAGS_H_
