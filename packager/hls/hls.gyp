# Copyright 2016 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'includes': [
    '../common.gypi',
  ],
  'targets': [
    {
      'target_name': 'hls_builder',
      'type': '<(component)',
      'sources': [
        'base/media_playlist.cc',
        'base/media_playlist.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../media/file/file.gyp:file',
        '../mpd/mpd.gyp:media_info_proto',
      ],
    },
    {
      'target_name': 'hls_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'base/media_playlist_unittest.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../media/test/media_test.gyp:run_tests_with_atexit_manager',
        '../mpd/mpd.gyp:media_info_proto',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'hls_builder',
      ],
    },
  ],
}
