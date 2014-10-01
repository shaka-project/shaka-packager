// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef APP_MPD_GENERATOR_FLAGS_H_
#define APP_MPD_GENERATOR_FLAGS_H_

#include <gflags/gflags.h>

DEFINE_string(input, "", "Comma separated list of MediaInfo input files.");
DEFINE_string(output, "", "MPD output file name.");
DEFINE_string(base_urls,
              "",
              "Comma separated BaseURLs for the MPD. The values will be added "
              "as <BaseURL> element(s) immediately under the <MPD> element.");
#endif  // APP_MPD_GENERATOR_FLAGS_H_
