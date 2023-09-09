# Copyright 2017 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(demuxer STATIC
  demuxer.cc
  demuxer.h)
target_link_libraries(demuxer
  media_base
  mp2t
  mp4
  webvtt
  wvm
  formats_webm
  media_origin)

add_executable(demuxer_unittest
  demuxer_unittest.cc
  )
target_link_libraries(demuxer_unittest
  demuxer
  file
  file_test_util
  test_data_util
  absl::flags
  media_event
  media_handler_test_base
  mock_muxer_listener
  gmock
  gtest
  gtest_main
)

add_test(NAME demuxer_unittest COMMAND demuxer_unittest)
