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
      'target_name': 'file',
      'type': '<(component)',
      'sources': [
        'callback_file.cc',
        'callback_file.h',
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
        'public/buffer_callback_params.h',
        'threaded_io_file.cc',
        'threaded_io_file.h',
        'udp_file.cc',
        'udp_file.h',
        'udp_options.cc',
        'udp_options.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/gflags/gflags.gyp:gflags',
      ],
    },
    {
      'target_name': 'file_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'callback_file_unittest.cc',
        'file_unittest.cc',
        'file_util_unittest.cc',
        'io_cache_unittest.cc',
        'memory_file_unittest.cc',
        'udp_options_unittest.cc',
      ],
      'dependencies': [
        '../media/test/media_test.gyp:run_tests_with_atexit_manager',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/gflags/gflags.gyp:gflags',
        'file',
      ],
    },
  ],
}
