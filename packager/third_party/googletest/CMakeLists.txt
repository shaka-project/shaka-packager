# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host gtest configuration.

# This is required, but doesn't have to correspond to the actual version we pin
# to.
set(GOOGLETEST_VERSION "1.12.0")

# Force gtest to use a shared CRT on MSVC.  Without this, we get link errors on
# our tests on Windows.
set(gtest_force_shared_crt ON)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source/googlemock)
