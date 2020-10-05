# Copyright 2020 Google LLC. All rights reserved.
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
      'target_name': 'dvb',
      'type': '<(component)',
      'sources': [
        'dvb_image.cc',
        'dvb_image.h',
        'dvb_sub_parser.cc',
        'dvb_sub_parser.h',
        'subtitle_composer.cc',
        'subtitle_composer.h',
      ],
      'dependencies': [
        '../../base/media_base.gyp:media_base',
        '../../../third_party/libpng/libpng.gyp:libpng',
      ],
    },
    {
      'target_name': 'dvb_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'dvb_image_unittest.cc',
        'dvb_sub_parser_unittest.cc',
        'subtitle_composer_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../event/media_event.gyp:mock_muxer_listener',
        '../../test/media_test.gyp:media_test_support',
        'dvb',
      ]
    },
  ],
}
