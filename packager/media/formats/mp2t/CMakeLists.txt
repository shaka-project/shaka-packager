# Copyright 2014 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(mp2t STATIC
  ac3_header.cc
  ac3_header.h
  adts_header.cc
  adts_header.h
  audio_header.h
  continuity_counter.cc
  continuity_counter.h
  es_parser_audio.cc
  es_parser_audio.h
  es_parser_dvb.cc
  es_parser_dvb.h
  es_parser_h264.cc
  es_parser_h264.h
  es_parser_h265.cc
  es_parser_h265.h
  es_parser_h26x.cc
  es_parser_h26x.h
  es_parser.h
  mp2t_media_parser.cc
  mp2t_media_parser.h
  mpeg1_header.cc
  mpeg1_header.h
  pes_packet.cc
  pes_packet.h
  pes_packet_generator.cc
  pes_packet_generator.h
  program_map_table_writer.cc
  program_map_table_writer.h
  ts_muxer.cc
  ts_muxer.h
  ts_packet.cc
  ts_packet.h
  ts_packet_writer_util.cc
  ts_packet_writer_util.h
  ts_section_pat.cc
  ts_section_pat.h
  ts_section_pes.cc
  ts_section_pes.h
  ts_section_pmt.cc
  ts_section_pmt.h
  ts_section_psi.cc
  ts_section_psi.h
  ts_segmenter.cc
  ts_segmenter.h
  ts_stream_type.h
  ts_writer.cc
  ts_writer.h
  )

target_link_libraries(mp2t
  absl::log
  dvb
  media_base
  media_codecs
  media_crypto
  hex_parser
)

add_executable(mp2t_unittest
ac3_header_unittest.cc
adts_header_unittest.cc
es_parser_h264_unittest.cc
es_parser_h26x_unittest.cc
mp2t_media_parser_unittest.cc
mpeg1_header_unittest.cc
pes_packet_generator_unittest.cc
program_map_table_writer_unittest.cc
ts_segmenter_unittest.cc
ts_writer_unittest.cc
  )

target_link_libraries(mp2t_unittest
  absl::flags
  absl::log
  file
  file_test_util
  gmock
  gtest
  gtest_main
  media_event
  media_handler_test_base
  mock_muxer_listener
  mp2t
  test_data_util)

add_test(NAME mp2t_unittest COMMAND mp2t_unittest)

