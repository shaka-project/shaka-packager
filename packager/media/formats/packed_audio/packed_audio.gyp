# Copyright 2018 Google LLC. All rights reserved.
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
      'target_name': 'packed_audio',
      'type': '<(component)',
      'sources': [
        'packed_audio_segmenter.cc',
        'packed_audio_segmenter.h',
      ],
      'dependencies': [
        '../../base/media_base.gyp:media_base',
        '../../codecs/codecs.gyp:codecs',
      ],
    },
    {
      'target_name': 'packed_audio_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'packed_audio_segmenter_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../codecs/codecs.gyp:codecs',
        '../../test/media_test.gyp:media_test_support',
        'packed_audio',
      ],
    },
  ],
}
