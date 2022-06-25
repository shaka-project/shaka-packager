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

# Tell third-party libraries not to build their own tests.
set(BUILD_TESTING OFF)

if(LIBPACKAGER_SHARED)
  add_definitions(SHARED_LIBRARY_BUILD)
endif()

# Subdirectories with their own CMakeLists.txt, all of whose targets are built.
add_subdirectory(file)
add_subdirectory(status)
add_subdirectory(tools)
add_subdirectory(version)

# Third-party subdirectories with their own CMakeLists.txt, whose targets are
# only built if they are referenced.
add_subdirectory(testing/googletest/googlemock EXCLUDE_FROM_ALL)
add_subdirectory(third_party/abseil-cpp EXCLUDE_FROM_ALL)
add_subdirectory(third_party/boringssl EXCLUDE_FROM_ALL)
add_subdirectory(third_party/curl EXCLUDE_FROM_ALL)
add_subdirectory(third_party/glog EXCLUDE_FROM_ALL)
add_subdirectory(third_party/json EXCLUDE_FROM_ALL)
