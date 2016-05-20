// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/version/version.h"

namespace shaka {

#if defined(PACKAGER_VERSION)
// PACKAGER_VERSION is generated in gyp file using script
// generate_version_string.py.
#if defined(NDEBUG)
const char kPackagerVersion[] = PACKAGER_VERSION "-release";
#else
const char kPackagerVersion[] = PACKAGER_VERSION "-debug";
#endif  // #if defined(NDEBUG)
#else
const char kPackagerVersion[] = "";
#endif  // #if defined(PACKAGER_VERSION)

}  // namespace shaka
