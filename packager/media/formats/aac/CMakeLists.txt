# Copyright 2024 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(aac STATIC
  aac_media_parser.cc
  aac_media_parser.h
  )

target_link_libraries(aac
  absl::log
  media_base
  media_codecs
  mp2t
  )

add_executable(aac_unittest
  aac_media_parser_unittest.cc
  )
target_link_libraries(aac_unittest
  aac
  file
  file_test_util
  test_data_util
  hex_parser
  absl::flags
  gmock
  gtest
  gtest_main
  )

add_test(NAME aac_unittest COMMAND aac_unittest)
