# Copyright 2023 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake policy settings.  These will be included before the project definition.
# NOTE: Flags like MSVC are not available at this level.

# Do not set default warning levels for MSVC.  We will choose the warning level.
cmake_policy(SET CMP0092 NEW)

# Use modern project versioning behavior to avoid a warning on newer CMake.
cmake_policy(SET CMP0048 NEW)

# No in-source builds allowed.
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
# TODO: Prevent accidental cmake invocation from deeper inside packager tree?

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
