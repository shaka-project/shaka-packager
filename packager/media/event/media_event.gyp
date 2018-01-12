# Copyright 2014 Google Inc. All rights reserved.
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
      'target_name': 'media_event',
      'type': '<(component)',
      'sources': [
        'combined_muxer_listener.cc',
        'combined_muxer_listener.h',
        'event_info.h',
        'hls_notify_muxer_listener.cc',
        'hls_notify_muxer_listener.h',
        'mpd_notify_muxer_listener.cc',
        'mpd_notify_muxer_listener.h',
        'muxer_listener.h',
        'muxer_listener_factory.cc',
        'muxer_listener_factory.h',
        'muxer_listener_internal.cc',
        'muxer_listener_internal.h',
        'vod_media_info_dump_muxer_listener.cc',
        'vod_media_info_dump_muxer_listener.h',
      ],
      'dependencies': [
        '../../file/file.gyp:file',
        '../../mpd/mpd.gyp:media_info_proto',
        # Depends on full protobuf to read/write with TextFormat.
        '../../third_party/protobuf/protobuf.gyp:protobuf_full_do_not_use',
        '../base/media_base.gyp:media_base',
        '../codecs/codecs.gyp:codecs',
      ],
    },
    {
      'target_name': 'mock_muxer_listener',
      'type': '<(component)',
      'sources': [
        'mock_muxer_listener.cc',
        'mock_muxer_listener.h',
      ],
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        'media_event',
      ],
    },
    {
      'target_name': 'media_event_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'hls_notify_muxer_listener_unittest.cc',
        'mpd_notify_muxer_listener_unittest.cc',
        'muxer_listener_test_helper.cc',
        'muxer_listener_test_helper.h',
        'vod_media_info_dump_muxer_listener_unittest.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../mpd/mpd.gyp:media_info_proto',
        '../../mpd/mpd.gyp:mpd_mocks',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        # Depends on full protobuf to read/write with TextFormat.
        '../../third_party/protobuf/protobuf.gyp:protobuf_full_do_not_use',
        '../test/media_test.gyp:run_tests_with_atexit_manager',
        'media_event',
      ],
    },
  ],
}
