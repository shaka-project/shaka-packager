# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'variables': {
    # Compile as chromium code to enable warnings and warnings-as-errors.
    'chromium_code': 1,
  },
  'target_defaults': {
    'include_dirs': [
      '.',
    ],
  },
  'targets': [
    {
      'target_name': 'packager_main',
      'type': 'executable',
      'sources': [
        'app/fixed_key_encryption_flags.h',
        'app/muxer_flags.h',
        'app/packager_main.cc',
        'app/widevine_encryption_flags.h',
      ],
      'dependencies': [
        'media/event/media_event.gyp:media_event',
        'media/file/file.gyp:file',
        'media/filters/filters.gyp:filters',
        'media/formats/mp2t/mp2t.gyp:mp2t',
        'media/formats/mp4/mp4.gyp:mp4',
        'media/formats/mpeg/mpeg.gyp:mpeg',
        'third_party/gflags/gflags.gyp:gflags',
      ],
    },
    {
      'target_name': 'mpd_generator',
      'type': 'executable',
      'sources': [
        'app/mpd_generator.cc',
        'app/mpd_generator_flags.h',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'mpd/mpd.gyp:mpd_util',
        'third_party/gflags/gflags.gyp:gflags',
      ],
    },
    {
      'target_name': 'packager_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'media/test/packager_test.cc',
      ],
      'dependencies': [
        'media/file/file.gyp:file',
        'media/filters/filters.gyp:filters',
        'media/formats/mp2t/mp2t.gyp:mp2t',
        'media/formats/mp4/mp4.gyp:mp4',
        'media/formats/mpeg/mpeg.gyp:mpeg',
        'media/test/media_test.gyp:media_test_support',
        'testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'media/base/media_base.gyp:*',
        'media/event/media_event.gyp:*',
        'media/file/file.gyp:*',
        'media/formats/mp2t/mp2t.gyp:*',
        'media/formats/mp4/mp4.gyp:*',
        'mpd/mpd.gyp:*',
      ],
    },
    {
      'target_name': 'packager_builder_tests',
      'type': 'none',
      'dependencies': [
        'media/base/media_base.gyp:media_base_unittest',
        'media/event/media_event.gyp:media_event_unittest',
        'media/file/file.gyp:file_unittest',
        'media/filters/filters.gyp:filters_unittest',
        'media/formats/mp2t/mp2t.gyp:mp2t_unittest',
        'media/formats/mp4/mp4.gyp:mp4_unittest',
        'mpd/mpd.gyp:mpd_unittest',
        'packager_test',
      ],
    },
  ],
}
