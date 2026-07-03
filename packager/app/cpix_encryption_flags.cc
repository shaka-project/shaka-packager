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
ABSL_FLAG(bool,
          enable_cpix_decryption,
          false,
          "Enable decryption with keys from a CPIX document. Keys are looked "
          "up by key ID, so the document's usage rules are not used.");
ABSL_FLAG(std::string, cpix, "", "Path or URL to the CPIX document.");
ABSL_FLAG(std::string,
          cpix_request_file,
          "",
          "Optional path to a CPIX request document. If set, --cpix must be "
          "an HTTP(S) URL; the request document is POSTed to it and the "
          "response is used as the CPIX document (SPEKE style exchange).");
ABSL_FLAG(std::string,
          cpix_headers,
          "",
          "Optional semicolon separated list of HTTP headers in 'Name: "
          "value' form to send when fetching the CPIX document, e.g. for "
          "authentication.");
ABSL_FLAG(std::string,
          cpix_private_key,
          "",
          "Optional path to the recipient RSA private key (PEM or DER), "
          "used to decrypt encrypted CPIX documents. Required when the "
          "document's content keys are encrypted.");

namespace shaka {

bool ValidateCpixCryptoFlags() {
  bool success = true;

  const bool cpix_crypto = absl::GetFlag(FLAGS_enable_cpix_encryption) ||
                           absl::GetFlag(FLAGS_enable_cpix_decryption);
  if (!ValidateFlag("cpix", absl::GetFlag(FLAGS_cpix), cpix_crypto,
                    /* optional= */ false,
                    "--enable_cpix_encryption/decryption")) {
    success = false;
  }
  if (!ValidateFlag("cpix_request_file", absl::GetFlag(FLAGS_cpix_request_file),
                    cpix_crypto,
                    /* optional= */ true,
                    "--enable_cpix_encryption/decryption")) {
    success = false;
  }
  if (!ValidateFlag("cpix_headers", absl::GetFlag(FLAGS_cpix_headers),
                    cpix_crypto, /* optional= */ true,
                    "--enable_cpix_encryption/decryption")) {
    success = false;
  }
  if (!ValidateFlag("cpix_private_key", absl::GetFlag(FLAGS_cpix_private_key),
                    cpix_crypto, /* optional= */ true,
                    "--enable_cpix_encryption/decryption")) {
    success = false;
  }
  return success;
}

}  // namespace shaka
