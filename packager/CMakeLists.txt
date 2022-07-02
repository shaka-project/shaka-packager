# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Packager CMake build file.

# Build static libs by default, or shared if LIBPACKAGER_SHARED is defined.
option(LIBPACKAGER_SHARED "Build libpackager as a shared library" OFF)
if(LIBPACKAGER_SHARED)
  add_definitions(-DSHARED_LIBRARY_BUILD)
endif()

# Subdirectories with their own CMakeLists.txt, all of whose targets are built.
add_subdirectory(file)
add_subdirectory(status)
add_subdirectory(third_party)
add_subdirectory(tools)
add_subdirectory(version)
