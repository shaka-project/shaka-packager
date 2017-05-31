# Copyright 2015 Google Inc. All rights reserved.
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
      'target_name': 'webvtt',
      'type': '<(component)',
      'sources': [
        'cue.cc',
        'cue.h',
        'webvtt_media_parser.cc',
        'webvtt_media_parser.h',
        'webvtt_sample_converter.cc',
        'webvtt_sample_converter.h',
        'webvtt_timestamp.cc',
        'webvtt_timestamp.h',
      ],
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../base/media_base.gyp:media_base',
        '../../formats/mp4/mp4.gyp:mp4',
      ],
    },
    {
      'target_name': 'webvtt_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'webvtt_media_parser_unittest.cc',
        'webvtt_sample_converter_unittest.cc',
        'webvtt_timestamp_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gmock.gyp:gmock',
        '../../../testing/gtest.gyp:gtest',
        '../../test/media_test.gyp:media_test_support',
        'webvtt',
      ]
    },
  ],
}
