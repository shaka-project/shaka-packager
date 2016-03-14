# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE filters or at
# https://developers.google.com/open-source/licenses/bsd

{
  'includes': [
    '../../common.gypi',
  ],
  'targets': [
    {
      'target_name': 'filters',
      'type': '<(component)',
      'sources': [
        'avc_decoder_configuration.cc',
        'avc_decoder_configuration.h',
        'decoder_configuration.cc',
        'decoder_configuration.h',
        'ec3_audio_util.cc',
        'ec3_audio_util.h',
        'h264_byte_to_unit_stream_converter.cc',
        'h264_byte_to_unit_stream_converter.h',
        'h264_parser.cc',
        'h264_parser.h',
        'h26x_bit_reader.cc',
        'h26x_bit_reader.h',
        'hevc_decoder_configuration.cc',
        'hevc_decoder_configuration.h',
        'nal_unit_to_byte_stream_converter.cc',
        'nal_unit_to_byte_stream_converter.h',
        'nalu_reader.cc',
        'nalu_reader.h',
        'vp_codec_configuration.cc',
        'vp_codec_configuration.h',
        'vp8_parser.cc',
        'vp8_parser.h',
        'vp9_parser.cc',
        'vp9_parser.h',
        'vpx_parser.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'filters_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'avc_decoder_configuration_unittest.cc',
        'ec3_audio_util_unittest.cc',
        'h264_byte_to_unit_stream_converter_unittest.cc',
        'h264_parser_unittest.cc',
        'h26x_bit_reader_unittest.cc',
        'hevc_decoder_configuration_unittest.cc',
        'nal_unit_to_byte_stream_converter_unittest.cc',
        'nalu_reader_unittest.cc',
        'vp_codec_configuration_unittest.cc',
        'vp8_parser_unittest.cc',
        'vp9_parser_unittest.cc',
      ],
      'dependencies': [
        '../../media/base/media_base.gyp:media_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../test/media_test.gyp:media_test_support',
        'filters',
      ],
    },
  ],
}
