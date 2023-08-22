# Copyright 2020 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(mp4 STATIC
  box.cc
  box.h
  box_buffer.h
  box_definitions.cc
  box_definitions.h
  box_reader.cc
  box_reader.h
  chunk_info_iterator.cc
  chunk_info_iterator.h
  composition_offset_iterator.cc
  composition_offset_iterator.h
  decoding_time_iterator.cc
  decoding_time_iterator.h
  fragmenter.cc
  fragmenter.h
  key_frame_info.h
  low_latency_segment_segmenter.cc
  low_latency_segment_segmenter.h
  mp4_media_parser.cc
  mp4_media_parser.h
  mp4_muxer.cc
  mp4_muxer.h
  multi_segment_segmenter.cc
  multi_segment_segmenter.h
  segmenter.cc
  segmenter.h
  single_segment_segmenter.cc
  single_segment_segmenter.h
  sync_sample_iterator.cc
  sync_sample_iterator.h
  track_run_iterator.cc
  track_run_iterator.h
  )
target_link_libraries(mp4
  media_base
  mpd_builder
  mbedtls
  media_codecs
  media_event
  absl::flags
  ttml
  )

add_executable(mp4_unittest
  box_definitions_unittest.cc
  box_reader_unittest.cc
  chunk_info_iterator_unittest.cc
  composition_offset_iterator_unittest.cc
  decoding_time_iterator_unittest.cc
  mp4_media_parser_unittest.cc
  sync_sample_iterator_unittest.cc
  track_run_iterator_unittest.cc
  )
target_link_libraries(mp4_unittest
  file
  file_test_util
  test_data_util
  absl::flags
  media_event
  mp4
  gmock
  gtest
  gtest_main
  )
add_test(NAME mp4_unittest COMMAND mp4_unittest)