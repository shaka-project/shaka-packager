# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'includes': [
    '../../common.gypi',
  ],
  'targets': [
    {
      'target_name': 'media_event',
      'type': '<(component)',
      'sources': [
        'mpd_notify_muxer_listener.cc',
        'mpd_notify_muxer_listener.h',
        'muxer_listener.h',
        'muxer_listener_internal.cc',
        'muxer_listener_internal.h',
        'vod_media_info_dump_muxer_listener.cc',
        'vod_media_info_dump_muxer_listener.h',
      ],
      'dependencies': [
        '../../mpd/mpd.gyp:media_info_proto',
        # Depends on full protobuf to read/write with TextFormat.
        '../../third_party/protobuf/protobuf.gyp:protobuf_full_do_not_use',
        '../base/media_base.gyp:base',
      ],
    },
    {
      'target_name': 'media_event_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'vod_media_info_dump_muxer_listener_unittest.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../mpd/mpd.gyp:media_info_proto',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
        # Depends on full protobuf to read/write with TextFormat.
        '../../third_party/protobuf/protobuf.gyp:protobuf_full_do_not_use',
        '../file/file.gyp:file',
        'media_event',
      ],
    },
  ],
}
