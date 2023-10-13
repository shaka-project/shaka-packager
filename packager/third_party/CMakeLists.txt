# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Packager CMake build file for third-party libraries.

# Tell third-party libraries generally not to build their own tests.
set(BUILD_TESTING OFF)

# Tell third-party libraries generally not to build shared library targets.
set(BUILD_SHARED_LIBS OFF)

# Tell CMake that we intend to override some libraries' options with set().
# By setting this default instead of using cmake_policy(SET CMP0077 NEW), we
# ensure that the defaults are reset when a library calls
# cmake_minimum_required.
# See https://gitlab.kitware.com/cmake/cmake/-/issues/20312
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(MSVC)
  # Use warning level 3 in third-party code, as opposed to level 4 used in our
  # own code.
  add_compile_options(/W3)
endif()

# These all use EXCLUDE_FROM_ALL so that only the referenced targets get built.
add_subdirectory(abseil-cpp EXCLUDE_FROM_ALL)
add_subdirectory(curl EXCLUDE_FROM_ALL)
add_subdirectory(googletest EXCLUDE_FROM_ALL)
add_subdirectory(json EXCLUDE_FROM_ALL)
add_subdirectory(libpng EXCLUDE_FROM_ALL)
add_subdirectory(libwebm EXCLUDE_FROM_ALL)
add_subdirectory(libxml2 EXCLUDE_FROM_ALL)
add_subdirectory(mbedtls EXCLUDE_FROM_ALL)
add_subdirectory(mongoose EXCLUDE_FROM_ALL)
add_subdirectory(protobuf EXCLUDE_FROM_ALL)
add_subdirectory(zlib EXCLUDE_FROM_ALL)
