// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for protection systems.

#include "packager/app/protection_system_flags.h"

DEFINE_string(
    protection_systems,
    "",
    "Protection systems to be generated. Supported protection systems include "
    "Widevine, PlayReady, FairPlay, Marlin and "
    "CommonSystem (https://goo.gl/s8RIhr).");
