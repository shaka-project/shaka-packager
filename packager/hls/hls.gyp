# Copyright 2016 Google Inc. All rights reserved.
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
      'target_name': 'hls_builder',
      'type': '<(component)',
      'sources': [
        'base/hls_notifier.h',
        'base/master_playlist.cc',
        'base/master_playlist.h',
        'base/media_playlist.cc',
        'base/media_playlist.h',
        'base/simple_hls_notifier.cc',
        'base/simple_hls_notifier.h',
        'base/tag.cc',
        'base/tag.h',
        'public/hls_params.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../file/file.gyp:file',
        '../media/base/media_base.gyp:media_base',
        '../media/base/media_base.gyp:widevine_pssh_data_proto',
        '../mpd/mpd.gyp:manifest_base',
        '../mpd/mpd.gyp:media_info_proto',
        '../third_party/gflags/gflags.gyp:gflags',
      ],
    },
    {
      'target_name': 'hls_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'base/master_playlist_unittest.cc',
        'base/media_playlist_unittest.cc',
        'base/mock_media_playlist.cc',
        'base/mock_media_playlist.h',
        'base/simple_hls_notifier_unittest.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../media/test/media_test.gyp:run_tests_with_atexit_manager',
        '../mpd/mpd.gyp:media_info_proto',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/gflags/gflags.gyp:gflags',
        'hls_builder',
      ],
    },
  ],
}
