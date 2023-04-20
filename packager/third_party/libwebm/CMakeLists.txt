# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host libwebm configuration.

# Enable warnings-as-errors.
set(ENABLE_WERROR ON)

# Disable these settings, none of which we need.
set(ENABLE_WEBMTS OFF)
set(ENABLE_WEBMINFO OFF)
set(ENABLE_TESTS OFF)
set(ENABLE_IWYU OFF)
set(ENABLE_WEBM_PARSER OFF)
set(MSVC_RUNTIME "dll")

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)

# libwebm headers have relative include paths that only get resolved with this.
target_include_directories(webm PUBLIC source)
