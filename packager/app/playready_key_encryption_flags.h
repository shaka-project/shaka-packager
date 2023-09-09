// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for PlayReady encryption.

#ifndef APP_PLAYREADY_KEY_ENCRYPTION_FLAGS_H_
#define APP_PLAYREADY_KEY_ENCRYPTION_FLAGS_H_

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

ABSL_DECLARE_FLAG(bool, enable_playready_encryption);
ABSL_DECLARE_FLAG(std::string, playready_server_url);
ABSL_DECLARE_FLAG(std::string, program_identifier);

namespace shaka {

/// Validate PlayReady encryption flags.
/// @return true on success, false otherwise.
bool ValidatePRCryptoFlags();

}  // namespace shaka

#endif  // APP_PLAYREADY_KEY_ENCRYPTION_FLAGS_H_
