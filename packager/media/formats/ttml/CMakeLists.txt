# Copyright 2020 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(ttml STATIC
  ttml_generator.cc
  ttml_generator.h
  ttml_muxer.cc
  ttml_muxer.h
  ttml_to_mp4_handler.cc
  ttml_to_mp4_handler.h
  )
target_link_libraries(ttml
  media_base
  mpd_builder
)

add_executable(ttml_unittest
  ttml_generator_unittest.cc
  )
target_link_libraries(ttml_unittest
  file
  file_test_util
  test_data_util
  absl::flags
  media_event
  ttml
  gmock
  gtest
  gtest_main
  )
add_test(NAME dvb_unittest COMMAND dvb_unittest)