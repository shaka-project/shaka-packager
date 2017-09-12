// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for protection systems.

#ifndef PACKAGER_APP_PROTECTION_SYSTEM_FLAGS_H_
#define PACKAGER_APP_PROTECTION_SYSTEM_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_bool(generate_common_pssh);
DECLARE_bool(generate_playready_pssh);
DECLARE_bool(generate_widevine_pssh);

#endif  // PACKAGER_APP_PROTECTION_SYSTEM_FLAGS_H_
