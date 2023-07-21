# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(media_chunking STATIC
    chunking_handler.cc
    cue_alignment_handler.cc
    sync_point_queue.cc
    text_chunker.cc
)
target_link_libraries(media_chunking
    media_base
)

add_executable(media_chunking_unittest
    chunking_handler_unittest.cc
    cue_alignment_handler_unittest.cc
    text_chunker_unittest.cc
)
target_link_libraries(media_chunking_unittest
    gmock
    gtest
    gtest_main
    media_chunking
    media_handler_test_base
)
add_gtest(media_chunking_unittest)
