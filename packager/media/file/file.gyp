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
        'file_closer.h',
        'io_cache.cc',
        'io_cache.h',
        'local_file.cc',
        'local_file.h',
        'threaded_io_file.cc',
        'threaded_io_file.h',
        'udp_file.cc',
        'udp_file.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/gflags/gflags.gyp:gflags',
        '../base/media_base.gyp:base',
      ],
    },
    {
      'target_name': 'file_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'file_unittest.cc',
        'io_cache_unittest.cc',
      ],
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
        'file',
      ],
    },
  ],
}
