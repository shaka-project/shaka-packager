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
      'target_name': 'run_tests_with_atexit_manager',
      'type': '<(component)',
      'sources': [
        'run_tests_with_atexit_manager.cc',
      ],
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'media_test_support',
      'type': '<(component)',
      'sources': [
        'test_data_util.cc',
        'test_data_util.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        'run_tests_with_atexit_manager',
      ],
    },
  ],
}
