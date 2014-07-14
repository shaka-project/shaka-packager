# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'variables': {
    # Compile as chromium code to enable warnings and warnings-as-errors.
    'chromium_code': 1,
  },
  'target_defaults': {
    'include_dirs': [
      '../../..',
    ],
  },
  'targets': [
    {
      'target_name': 'wvm',
      'type': '<(component)',
      'sources': [
        'wvm_media_parser.cc',
        'wvm_media_parser.h',
      ],
      'dependencies': [
        '../../base/media_base.gyp:base',
      ],
    },
#    {
#      'target_name': 'wvm_unittest',
#      'type': '<(gtest_target_type)',
#      'sources': [
#        'adts_header_unittest.cc',
#        'es_parser_h264_unittest.cc',
#        'wvm_media_parser_unittest.cc',
#      ],
#      'dependencies': [
#        '../../../testing/gtest.gyp:gtest',
#        '../../../testing/gmock.gyp:gmock',
#        '../../filters/filters.gyp:filters',
#        '../../test/media_test.gyp:media_test_support',
#        '../mpeg/mpeg.gyp:mpeg',
#        'wvm',
#      ]
#   },
  ],
}
