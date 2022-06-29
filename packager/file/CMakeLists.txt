# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(file STATIC
    callback_file.cc
    file.cc
    file_util.cc
    http_file.cc
    io_cache.cc
    local_file.cc
    memory_file.cc
    thread_pool.cc
    threaded_io_file.cc
    udp_file.cc
    udp_options.cc)
target_link_libraries(file
    absl::base
    absl::flags
    absl::str_format
    absl::strings
    absl::synchronization
    absl::time
    libcurl
    glog
    status
    version
    -lstdc++fs)

add_executable(file_unittest
    callback_file_unittest.cc
    file_unittest.cc
    file_util_unittest.cc
    http_file_unittest.cc
    io_cache_unittest.cc
    memory_file_unittest.cc
    udp_options_unittest.cc)
target_link_libraries(file_unittest
    file
    gmock
    gtest
    gtest_main
    nlohmann_json)
add_test(NAME file_unittest COMMAND file_unittest)
