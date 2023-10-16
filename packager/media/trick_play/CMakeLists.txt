# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(media_trick_play STATIC
    trick_play_handler.cc)
target_link_libraries(media_trick_play
    absl::base
    absl::log
    media_base)

add_executable(media_trick_play_unittest
    trick_play_handler_unittest.cc)
target_link_libraries(media_trick_play_unittest
    media_base
    media_trick_play
    media_handler_test_base
    status
    gmock
    gtest
    gtest_main)
add_gtest(media_trick_play_unittest)
