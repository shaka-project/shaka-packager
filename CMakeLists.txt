# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Root-level CMake build file.

# Minimum CMake version.
cmake_minimum_required(VERSION 3.10)

# No in-source builds allowed.
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Minimum C++ version.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Global C++ flags.
set(CMAKE_CXX_FLAGS "-Wall")

# Global include paths.

# Project root, to reference packager/foo/bar/...
include_directories(.)

# Project name.  May not contain spaces.
project(shaka-packager)

# Build options.
option(LIBPACKAGER_SHARED "Build libpackager as a shared library")

# Subdirectories with their own CMakeLists.txt
add_subdirectory(packager)
