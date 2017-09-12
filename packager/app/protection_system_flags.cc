// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for protection systems.

#include "packager/app/protection_system_flags.h"

DEFINE_bool(generate_common_pssh,
            false,
            "When specified, generate an additional v1 PSSH box for the common "
            "system ID. See: https://goo.gl/s8RIhr."
            "The flag is default to be true if --enable_raw_key_encryption "
            "is set and no other pssh flags are specified.");
DEFINE_bool(generate_playready_pssh,
            false,
            "When specified, include a Playready PSSH box."
            "A playready PSSH is always generated regardless of the value of "
            "--generate_playready_pssh for --enable_playready_encryption.");
DEFINE_bool(generate_widevine_pssh,
            false,
            "When specified, include a Widevine PSSH box. "
            "A widevine PSSH is always generated regardless of the value of "
            "--generate_widevine_pssh for --enable_widevine_encryption.");
