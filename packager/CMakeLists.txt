# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Packager CMake build file.

# Build static libs by default, or shared if LIBPACKAGER_SHARED is defined.
if(LIBPACKAGER_SHARED)
  add_definitions(-DSHARED_LIBRARY_BUILD)
endif()

# Global C++ flags.
if(MSVC)
  # Warning level 4 and all warnings as errors.
  add_compile_options(/W4 /WX)
else()
  # Lots of warnings and all warnings as errors.
  # Note that we can't use -Wpedantic due to absl's int128 headers.
  add_compile_options(-Wall -Wextra -Werror)
endif()

# Subdirectories with their own CMakeLists.txt, all of whose targets are built.
add_subdirectory(file)
add_subdirectory(status)
add_subdirectory(third_party)
add_subdirectory(tools)
add_subdirectory(version)
