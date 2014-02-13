# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'include_dirs': [
      '../..',
    ],
  },
  'targets': [
    {
      'target_name': 'media_test_support',
      'type': '<(component)',
      'sources': [
        'run_tests_with_atexit_manager.cc',
        'test_data_util.cc',
        'test_data_util.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
      ],
    },
  ],
}
