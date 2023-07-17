# Copyright 2022 Google LLC. All rights reserved.
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
  # Silence a warning from an absl header about alignment in boolean flags.
  add_compile_options(/wd4324)
  # Silence a warning about STL types in exported classes.
  add_compile_options(/wd4251)
  # Silence a warning about constant conditional expressions.
  add_compile_options(/wd4127)

  # Packager's macro for Windows-specific code.
  add_definitions(-DOS_WIN)

  # Suppress Microsoft's min() and max() macros, which will conflict with
  # things like std::numeric_limits::max() and std::min().
  add_definitions(-DNOMINMAX)
  # Define this so that we can use fopen() without warnings.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  # Don't automatically include winsock.h in windows.h.  This is needed for us
  # to use winsock2.h, which contains definitions that conflict with the
  # ancient winsock 1.1 interface in winsock.h.
  add_definitions(-DWIN32_LEAN_AND_MEAN)
else()
  # Lots of warnings and all warnings as errors.
  # Note that we can't use -Wpedantic due to absl's int128 headers.
  add_compile_options(-Wall -Wextra -Werror)
  # Several warning suppression flags are required on one compiler version and
  # not understood by another.  Do not treat these as errors.
  add_compile_options(-Wno-unknown-warning-option)
endif()

# Include our module for gtest-based testing.
include("gtest.cmake")

# Include our module for building protos.
include("protobuf.cmake")

# Subdirectories with their own CMakeLists.txt, all of whose targets are built.
add_subdirectory(file)
add_subdirectory(kv_pairs)
add_subdirectory(media)
add_subdirectory(mpd)
add_subdirectory(status)
add_subdirectory(third_party)
add_subdirectory(tools)
add_subdirectory(utils)
add_subdirectory(version)
