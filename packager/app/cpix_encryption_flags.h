// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for CPIX encryption.

#ifndef PACKAGER_APP_CPIX_ENCRYPTION_FLAGS_H_
#define PACKAGER_APP_CPIX_ENCRYPTION_FLAGS_H_

#include <string>

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

ABSL_DECLARE_FLAG(bool, enable_cpix_encryption);
ABSL_DECLARE_FLAG(std::string, cpix);

namespace shaka {

/// Validate CPIX encryption flags.
/// @return true on success, false otherwise.
bool ValidateCpixCryptoFlags();

}  // namespace shaka

#endif  // PACKAGER_APP_CPIX_ENCRYPTION_FLAGS_H_
