# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Packager CMake build file.

# Variables that must be defined for third-party libraries.

# NOTE: This doesn't have to correspond to the actual version we pin to.
set(GOOGLETEST_VERSION "1.12.0")

# Subdirectories with their own CMakeLists.txt
add_subdirectory(testing/googletest/googlemock)  # includes gtest
add_subdirectory(tools)
add_subdirectory(version)
