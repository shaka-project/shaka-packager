# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(media_codecs STATIC
    aac_audio_specific_config.cc
    ac3_audio_util.cc
    av1_codec_configuration_record.cc
    av1_parser.cc
    avc_decoder_configuration_record.cc
    decoder_configuration_record.cc
    dovi_decoder_configuration_record.cc
    dts_audio_specific_config.cc
    ec3_audio_util.cc
    ac4_audio_util.cc
    es_descriptor.cc
    h264_byte_to_unit_stream_converter.cc
    h264_parser.cc
    h265_byte_to_unit_stream_converter.cc
    h265_parser.cc
    h26x_bit_reader.cc
    h26x_byte_to_unit_stream_converter.cc
    hevc_decoder_configuration_record.cc
    hls_audio_util.cc
    nal_unit_to_byte_stream_converter.cc
    nalu_reader.cc
    video_slice_header_parser.cc
    vp_codec_configuration_record.cc
    vp8_parser.cc
    vp9_parser.cc
)

target_link_libraries(media_codecs
    media_base)

add_executable(media_codecs_unittest
    aac_audio_specific_config_unittest.cc
    ac3_audio_util_unittest.cc
    av1_codec_configuration_record_unittest.cc
    av1_parser_unittest.cc
    avc_decoder_configuration_record_unittest.cc
    dovi_decoder_configuration_record_unittest.cc
    ec3_audio_util_unittest.cc
    ac4_audio_util_unittest.cc
    es_descriptor_unittest.cc
    h264_byte_to_unit_stream_converter_unittest.cc
    h264_parser_unittest.cc
    h265_byte_to_unit_stream_converter_unittest.cc
    h265_parser_unittest.cc
    h26x_bit_reader_unittest.cc
    hevc_decoder_configuration_record_unittest.cc
    hls_audio_util_unittest.cc
    nal_unit_to_byte_stream_converter_unittest.cc
    nalu_reader_unittest.cc
    video_slice_header_parser_unittest.cc
    vp_codec_configuration_record_unittest.cc
    vp8_parser_unittest.cc
    vp9_parser_unittest.cc
)

target_link_libraries(media_codecs_unittest
    gmock
    gtest
    gtest_main
    media_codecs
    test_data_util)

add_gtest(media_codecs_unittest)
