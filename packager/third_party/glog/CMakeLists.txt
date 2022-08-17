# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host glog configuration.

# We will use abseil for flags, so turn off glog's search for gflags.
set(WITH_GFLAGS OFF)

# Don't depend on gtest, since we won't be building glog's tests.
set(WITH_GTEST OFF)

# Don't depend on or search for libunwind.
set(WITH_UNWIND OFF)

# Disable this warning/error, which occurs in demangle.c on macOS:
if(NOT MSVC)
  add_compile_options(-Wno-shorten-64-to-32)
endif()

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
