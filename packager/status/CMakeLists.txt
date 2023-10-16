# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(status STATIC
    status.cc)
target_link_libraries(status
    absl::log
    absl::str_format)

if(LIBPACKAGER_SHARED)
  target_compile_definitions(status PUBLIC SHAKA_IMPLEMENTATION)
endif()

add_executable(status_unittest
    status_test_util_unittest.cc
    status_unittest.cc)
target_link_libraries(status_unittest
    status
    gmock
    gtest
    gtest_main)
add_gtest(status_unittest)
