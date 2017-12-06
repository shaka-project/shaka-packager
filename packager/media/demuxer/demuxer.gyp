# Copyright 2017 Google Inc. All rights reserved.
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
      'target_name': 'demuxer',
      'type': '<(component)',
      'sources': [
        'demuxer.cc',
        'demuxer.h',
      ],
      'dependencies': [
        '../base/media_base.gyp:media_base',
        '../formats/mp2t/mp2t.gyp:mp2t',
        '../formats/mp4/mp4.gyp:mp4',
        '../formats/webm/webm.gyp:webm',
        '../formats/webvtt/webvtt.gyp:webvtt',
        '../formats/wvm/wvm.gyp:wvm',
        '../origin/origin.gyp:origin',
      ],
    },
    {
      'target_name': 'demuxer_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'demuxer_unittest.cc',
      ],
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../base/media_base.gyp:media_handler_test_base',
        '../test/media_test.gyp:media_test_support',
        'demuxer',
      ]
    },
  ],
}
