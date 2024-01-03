# Copyright 2018 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(packed_audio STATIC
  packed_audio_segmenter.cc
  packed_audio_segmenter.h
  packed_audio_writer.cc
  packed_audio_writer.h
  )

target_link_libraries(packed_audio
  media_event
  media_codecs
)

add_executable(packed_audio_unittest
  packed_audio_segmenter_unittest.cc
  packed_audio_writer_unittest.cc
  )
target_link_libraries(packed_audio_unittest
  file
  file_test_util
  mock_muxer_listener
  media_handler_test_base
  test_data_util
  absl::flags
  media_event
  packed_audio
  gmock
  gtest
  gtest_main
  )

add_test(NAME packed_audio_unittest COMMAND packed_audio_unittest)