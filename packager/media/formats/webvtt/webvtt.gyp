# Copyright 2015 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'variables': {
    'shaka_code': 1,
  },
  'targets': [
    {
      'target_name': 'webvtt',
      'type': '<(component)',
      'sources': [
        'text_padder.cc',
        'text_padder.h',
        'text_readers.cc',
        'text_readers.h',
        'webvtt_file_buffer.cc',
        'webvtt_file_buffer.h',
        'webvtt_muxer.cc',
        'webvtt_muxer.h',
        'webvtt_parser.cc',
        'webvtt_parser.h',
        'webvtt_to_mp4_handler.cc',
        'webvtt_to_mp4_handler.h',
        'webvtt_utils.cc',
        'webvtt_utils.h',
      ],
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../base/media_base.gyp:media_base',
        '../../formats/mp4/mp4.gyp:mp4',
        '../../origin/origin.gyp:origin',
      ],
    },
    {
      'target_name': 'webvtt_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'text_readers_unittest.cc',
        'webvtt_muxer_unittest.cc',
        'webvtt_parser_unittest.cc',
        'webvtt_utils_unittest.cc',
        'webvtt_to_mp4_handler_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gmock.gyp:gmock',
        '../../../testing/gtest.gyp:gtest',
        '../../base/media_base.gyp:media_handler_test_base',
        '../../event/media_event.gyp:mock_muxer_listener',
        '../../test/media_test.gyp:media_test_support',
        'webvtt',
      ]
    },
  ],
}
