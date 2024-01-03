// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef APP_MPD_GENERATOR_FLAGS_H_
#define APP_MPD_GENERATOR_FLAGS_H_

#include <absl/flags/flag.h>

ABSL_FLAG(std::string,
          input,
          "",
          "Comma separated list of MediaInfo input files.");
ABSL_FLAG(std::string, output, "", "MPD output file name.");
ABSL_FLAG(std::string,
          base_urls,
          "",
          "Comma separated BaseURLs for the MPD. The values will be added "
          "as <BaseURL> element(s) immediately under the <MPD> element.");
#endif  // APP_MPD_GENERATOR_FLAGS_H_
