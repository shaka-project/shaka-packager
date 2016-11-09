// Copyright 2016 Inside Secure Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for PlayReady encryption.

#ifndef APP_PLAYREADY_ENCRYPTION_FLAGS_H_
#define APP_PLAYREADY_ENCRYPTION_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_bool(enable_playready_encryption);
DECLARE_string(pr_key_id);
DECLARE_string(pr_key);
DECLARE_string(pr_iv);
DECLARE_string(pr_additional_key_ids);
DECLARE_string(pr_la_url);
DECLARE_string(pr_lui_url);
DECLARE_bool(pr_ondemand);
DECLARE_bool(pr_include_empty_license_store);

namespace shaka {

/// Validate playready encryption flags.
/// @return true on success, false otherwise.
bool ValidatePlayreadyCryptoFlags();

}  // namespace shaka

#endif  // APP_PLAYREADY_ENCRYPTION_FLAGS_H_
