# Copyright 2023 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host c-ares configuration.

set(CARES_STATIC ON)
set(CARES_SHARED OFF)
set(CARES_INSTALL OFF)
set(CARES_STATIC_PIC OFF)
set(CARES_BUILD_TESTS OFF)
set(CARES_BUILD_CONTAINER_TESTS OFF)
set(CARES_BUILD_TOOLS OFF)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
