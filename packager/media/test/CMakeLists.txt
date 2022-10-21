# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(run_tests_with_atexit_manager STATIC
    run_tests_with_atexit_manager.cc
target_link_libraries(run_tests_with_atexit_manager
    gtest
    version)

add_executable(media_test_support
    test_data_util.cc
target_link_libraries(media_test_support
    run_tests_with_atexit_manager
    gmock
    gtest
    gtest_main
    nlohmann_json)
add_test(NAME media_test_support COMMAND media_test_support)
