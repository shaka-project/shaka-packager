# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(kv_pairs STATIC
    kv_pairs.cc)
target_link_libraries(kv_pairs
    absl::log
    absl::str_format)

add_executable(kv_pairs_unittest
    kv_pairs_unittest.cc)
target_link_libraries(kv_pairs_unittest
    kv_pairs
    gmock
    gtest
    gtest_main)
add_gtest(kv_pairs_unittest)
