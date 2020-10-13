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
      'target_name': 'ttml',
      'type': '<(component)',
      'sources': [
        'ttml_generator.cc',
        'ttml_generator.h',
        'ttml_muxer.cc',
        'ttml_muxer.h',
        'ttml_to_mp4_handler.cc',
        'ttml_to_mp4_handler.h',
      ],
      'dependencies': [
        '../../base/media_base.gyp:media_base',
        '../../../mpd/mpd.gyp:mpd_builder',
      ],
      'export_dependent_settings': [
        '../../../mpd/mpd.gyp:mpd_builder',
      ],
    },
    {
      'target_name': 'ttml_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'ttml_generator_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../../third_party/libxml/libxml.gyp:libxml',
        '../../base/media_base.gyp:media_handler_test_base',
        '../../event/media_event.gyp:mock_muxer_listener',
        '../../test/media_test.gyp:media_test_support',
        'ttml',
      ]
    },
  ],
}
