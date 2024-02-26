# Copyright 2024 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host mimalloc configuration.
# This is only used to produce static binaries on Linux, as a replacement for
# the default allocator in musl, which is slower.

# Turn these off to save time.
set(MI_BUILD_SHARED OFF)
set(MI_BUILD_STATIC OFF)
set(MI_BUILD_TESTS OFF)

# Turn these on.  They are already on by default as of the date we wrote this
# file, but in case the defaults ever change, these settings are critical.
set(MI_OVERRIDE ON)
set(MI_BUILD_OBJECT ON)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
