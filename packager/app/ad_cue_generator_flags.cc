// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines cuepoint generator flags.

#include "packager/app/ad_cue_generator_flags.h"

DEFINE_string(ad_cues,
              "",
              "List of cuepoint markers."
              "This flag accepts semicolon separated pairs and components in "
              "the pair are separated by a comma and the second component "
              "duration is optional. For example --ad_cues "
              "{start_time}[,{duration}][;{start_time}[,{duration}]]..."
              "The start_time represents the start of the cue marker in "
              "seconds relative to the start of the program.");
