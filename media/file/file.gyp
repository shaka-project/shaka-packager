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
      'target_name': 'file',
      'type': '<(component)',
      'sources': [
        'file.cc',
        'file.h',
        'file_closer.h',
        'local_file.cc',
        'local_file.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'file_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'file_unittest.cc',
      ],
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
        'file',
      ],
    },
  ],
}
