# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host mbedtls configuration.

# Don't build executables or tests from mbedtls.
set(ENABLE_PROGRAMS OFF)
set(ENABLE_TESTING OFF)

# Don't generate any files, which would mean a build-time dependency on Perl.
set(GEN_FILES OFF)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
