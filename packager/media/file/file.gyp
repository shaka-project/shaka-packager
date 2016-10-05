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
      'target_name': 'file',
      'type': '<(component)',
      'sources': [
        'file.cc',
        'file.h',
        'file_util.cc',
        'file_util.h',
        'file_closer.h',
        'io_cache.cc',
        'io_cache.h',
        'local_file.cc',
        'local_file.h',
        'memory_file.cc',
        'memory_file.h',
        'threaded_io_file.cc',
        'threaded_io_file.h',
        'udp_file.h',
        'udp_options.cc',
        'udp_options.h',
      ],
      'conditions': [
        ['OS == "win"', {
          'sources': [
            'udp_file_win.cc',
          ],
        }, {
          'sources': [
            'udp_file_posix.cc',
          ],
        }],
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/gflags/gflags.gyp:gflags',
        '../base/media_base.gyp:media_base',
      ],
    },
    {
      'target_name': 'file_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'file_unittest.cc',
        'file_util_unittest.cc',
        'io_cache_unittest.cc',
        'memory_file_unittest.cc',
        'udp_options_unittest.cc',
      ],
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        '../../third_party/gflags/gflags.gyp:gflags',
        '../test/media_test.gyp:run_tests_with_atexit_manager',
        'file',
      ],
    },
  ],
}
