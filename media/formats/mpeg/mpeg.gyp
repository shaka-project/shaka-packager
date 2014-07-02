# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'includes': [
    '../../../common.gypi',
  ],
  'targets': [
    {
      'target_name': 'mpeg',
      'type': '<(component)',
      'sources': [
        'adts_constants.cc',
        'adts_constants.h',
      ],
    },
    {
      'target_name': 'mpeg_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
      ],
      'dependencies': [
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../test/media_test.gyp:media_test_support',
        'mpeg',
      ]
    },
  ],
}
