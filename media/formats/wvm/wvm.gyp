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
      'target_name': 'wvm',
      'type': '<(component)',
      'sources': [
        'wvm_media_parser.cc',
        'wvm_media_parser.h',
      ],
      'dependencies': [
        '../../base/media_base.gyp:base',
        '../../filters/filters.gyp:filters',
        '../../formats/mp2t/mp2t.gyp:mp2t',
        '../mpeg/mpeg.gyp:mpeg',
      ],
    },
    {
      'target_name': 'wvm_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'wvm_media_parser_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../test/media_test.gyp:media_test_support',
        'wvm',
      ]
    },
  ],
}
