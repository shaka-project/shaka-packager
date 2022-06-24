# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Packager CMake build file.

# Variables that must be defined for third-party libraries.

# NOTE: This doesn't have to correspond to the actual version we pin to.
set(GOOGLETEST_VERSION "1.12.0")

# This will be the new default, so turn it on now to suppress a warning.
set(ABSL_PROPAGATE_CXX_STD ON)

# We will use abseil for flags.
set(WITH_GFLAGS OFF)

if(LIBPACKAGER_SHARED)
  add_definitions(SHARED_LIBRARY_BUILD)
endif()

# Subdirectories with their own CMakeLists.txt
add_subdirectory(status)
add_subdirectory(testing/googletest/googlemock)  # includes gtest
add_subdirectory(third_party/abseil-cpp)
add_subdirectory(third_party/boringssl)
add_subdirectory(third_party/curl)
add_subdirectory(third_party/glog)
add_subdirectory(tools)
add_subdirectory(version)
