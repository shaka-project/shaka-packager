// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef SHAKA_PACKAGER_STRING_TRIM_SPLIT_H

#include <string>
#include <vector>

namespace shaka {
std::vector<std::string> SplitAndTrimSkipEmpty(const std::string& str,
                                               char delimiter);
}

#define SHAKA_PACKAGER_STRING_TRIM_SPLIT_H

#endif  // SHAKA_PACKAGER_STRING_TRIM_SPLIT_H
