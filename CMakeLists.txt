# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Root-level CMake build file.

# Project name.  May not contain spaces.  Versioning is managed elsewhere.
cmake_policy(SET CMP0048 NEW)
project(shaka-packager VERSION "")

# Minimum CMake version.
# We could require as low as 3.10, but glog requires 3.16.
cmake_minimum_required(VERSION 3.16)

# The only build option for Shaka Packager is whether to build a shared
# libpackager library.  By default, don't.
option(LIBPACKAGER_SHARED "Build libpackager as a shared library" OFF)

# No in-source builds allowed.
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Minimum C++ version.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Minimum GCC version, if using GCC.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  # Require at least GCC 9.  Before GCC 9, C++17 filesystem libs don't link.
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    message(FATAL_ERROR "GCC version must be at least 9! (Found ${CMAKE_CXX_COMPILER_VERSION})")
  endif()
endif()

# Global include paths.
# Project root, to reference packager/foo/bar/...
include_directories(.)

# Enable CMake's test infrastructure.
enable_testing()

# Subdirectories with their own CMakeLists.txt
add_subdirectory(packager)
