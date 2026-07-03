// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for CPIX encryption.

#include <packager/app/cpix_encryption_flags.h>

#include <packager/app/validate_flag.h>

ABSL_FLAG(bool,
          enable_cpix_encryption,
          false,
          "Enable encryption with keys from a CPIX document (DASH-IF Content "
          "Protection Information Exchange). Keys, DRM signaling (PSSH) and "
          "key to stream mapping are read from the document. Note that DRM "
          "signaling comes from the document's DRMSystemList; "
          "--protection_systems may be used to generate signaling for "
          "additional protection systems.");
ABSL_FLAG(std::string, cpix, "", "Path or URL to the CPIX document.");

namespace shaka {

bool ValidateCpixCryptoFlags() {
  bool success = true;

  const bool cpix_crypto = absl::GetFlag(FLAGS_enable_cpix_encryption);
  if (!ValidateFlag("cpix", absl::GetFlag(FLAGS_cpix), cpix_crypto,
                    /* optional= */ false, "--enable_cpix_encryption")) {
    success = false;
  }
  return success;
}

}  // namespace shaka
