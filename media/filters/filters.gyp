# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE filters or at
# https://developers.google.com/open-source/licenses/bsd

{
  'variables': {
    # Compile as chromium code to enable warnings and warnings-as-errors.
    'chromium_code': 1,
  },
  'target_defaults': {
    'include_dirs': [
      '../..',
    ],
  },
  'targets': [
    {
      'target_name': 'filters',
      'type': '<(component)',
      'sources': [
        'h264_bit_reader.cc',
        'h264_bit_reader.h',
        'h264_parser.cc',
        'h264_parser.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'filters_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'h264_bit_reader_unittest.cc',
        'h264_parser_unittest.cc',
      ],
      'dependencies': [
        '../../media/base/media_base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        '../test/media_test.gyp:media_test_support',
        'filters',
      ],
    },
  ],
}
