// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <string>

namespace shaka {

/// @return URL of shaka-packager project.
std::string GetPackagerProjectUrl();

/// @return The version string.
std::string GetPackagerVersion();

/// Set version for testing.
/// @param version contains the injected testing version.
void SetPackagerVersionForTesting(const std::string& version);

}  // namespace shaka
