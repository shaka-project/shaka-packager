# Copyright 2014 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(wvm STATIC
  wvm_media_parser.cc
  wvm_media_parser.h
  )

target_link_libraries(wvm
  media_base
  media_codecs
  mp2t
  glog
)

add_executable(wvm_unittest
  wvm_media_parser_unittest.cc
  )

target_link_libraries(wvm_unittest
  wvm
  file
  file_test_util
  test_data_util
  absl::flags
  media_event
  media_handler_test_base
  mock_muxer_listener
  glog
  gmock
  gtest
  gtest_main
  )

add_test(NAME wvm_unittest COMMAND wvm_unittest)
