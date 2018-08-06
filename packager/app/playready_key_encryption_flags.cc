// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for PlayReady encryption.

#include "packager/app/playready_key_encryption_flags.h"

#include "packager/app/validate_flag.h"

DEFINE_bool(enable_playready_encryption,
            false,
            "Enable encryption with PlayReady key.");
DEFINE_string(playready_server_url, "", "PlayReady packaging server url.");
DEFINE_string(program_identifier, "",
              "Program identifier for packaging request.");
DEFINE_string(ca_file, "",
              "Absolute path to the Certificate Authority file for the "
              "server cert. PEM format");
DEFINE_string(client_cert_file, "",
              "Absolute path to client certificate file.");
DEFINE_string(client_cert_private_key_file, "",
              "Absolute path to the Private Key file.");
DEFINE_string(client_cert_private_key_password, "",
              "Password to the private key file.");

namespace shaka {
namespace {
const bool kFlagIsOptional = true;
}

bool ValidatePRCryptoFlags() {
  bool success = true;

  const char playready_label[] = "--enable_playready_encryption";
  bool playready_enabled = FLAGS_enable_playready_encryption;
  if (!ValidateFlag("playready_server_url", FLAGS_playready_server_url,
                    playready_enabled, !kFlagIsOptional, playready_label)) {
    success = false;
  }
  if (!ValidateFlag("program_identifier", FLAGS_program_identifier,
                    playready_enabled, !kFlagIsOptional, playready_label)) {
    success = false;
  }
  return success;
}

}  // namespace shaka
