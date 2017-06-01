// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines retired / deprecated flags. These flags will be removed in later
// versions.

#include "packager/app/retired_flags.h"

#include <stdio.h>

DEFINE_string(profile, "", "This flag is deprecated. Do not use.");
DEFINE_bool(single_segment, true, "This flag is deprecated. Do not use.");
DEFINE_bool(webm_subsample_encryption,
            true,
            "This flag is deprecated. Use vp9_subsample_encryption instead.");
DEFINE_double(availability_time_offset,
              0,
              "This flag is deprecated. Use suggested_presentation_delay "
              "instead which can achieve similar effect.");

// The current gflags library does not provide a way to check whether a flag is
// set in command line. If a flag has a different value to its default value,
// the flag must have been set. It is possible that the flag is set to the same
// value as its default value though.
bool InformRetiredStringFlag(const char* flagname, const std::string& value) {
  if (!value.empty())
    fprintf(stderr, "WARNING: %s is deprecated and ignored.\n", flagname);
  return true;
}

bool InformRetiredDefaultTrueFlag(const char* flagname, bool value) {
  if (!value)
    fprintf(stderr, "WARNING: %s is deprecated and ignored.\n", flagname);
  return true;
}

bool InformRetiredDefaultDoubleFlag(const char* flagname, double value) {
  if (value != 0)
    fprintf(stderr, "WARNING: %s is deprecated and ignored.\n", flagname);
  return true;
}

DEFINE_validator(profile, &InformRetiredStringFlag);
DEFINE_validator(single_segment, &InformRetiredDefaultTrueFlag);
DEFINE_validator(webm_subsample_encryption, &InformRetiredDefaultTrueFlag);
DEFINE_validator(availability_time_offset, &InformRetiredDefaultDoubleFlag);
