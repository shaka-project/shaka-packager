# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Root-level CMake build file.

# Minimum CMake version.  This must be in the root level CMakeLists.txt.
cmake_minimum_required(VERSION 3.16)

# These policy settings should be included before the project definition.
include("packager/policies.cmake")

# Project name.  May not contain spaces.  Versioning is managed elsewhere.
project(shaka-packager VERSION "")

# The only build option for Shaka Packager is whether to build a shared
# libpackager library.  By default, don't.
option(LIBPACKAGER_SHARED "Build libpackager as a shared library" OFF)

# Enable CMake's test infrastructure.
enable_testing()

# Subdirectories with their own CMakeLists.txt
add_subdirectory(packager)
add_subdirectory(link-test)
