# Copyright 2020 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(webvtt STATIC
  text_padder.cc
  text_padder.h
  text_readers.cc
  text_readers.h
  webvtt_file_buffer.cc
  webvtt_file_buffer.h
  webvtt_muxer.cc
  webvtt_muxer.h
  webvtt_parser.cc
  webvtt_parser.h
  webvtt_to_mp4_handler.cc
  webvtt_to_mp4_handler.h
  webvtt_utils.cc
  webvtt_utils.h
  )
target_link_libraries(webvtt
  media_base
  mp4
  media_origin
  string_utils
  )

add_executable(webvtt_unittest
  text_readers_unittest.cc
  webvtt_muxer_unittest.cc
  webvtt_parser_unittest.cc
  webvtt_utils_unittest.cc
  webvtt_to_mp4_handler_unittest.cc
  )
target_link_libraries(webvtt_unittest
  file
  file_test_util
  test_data_util
  absl::flags
  media_event
  media_handler_test_base
  mock_muxer_listener
  webvtt
  gmock
  gtest
  gtest_main
  )
add_test(NAME webvtt_unittest COMMAND webvtt_unittest)
