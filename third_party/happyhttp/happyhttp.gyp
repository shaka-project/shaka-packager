# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'happyhttp_lib',
      'type': 'static_library',
      'sources': [
        'src/happyhttp.cpp',
        'src/happyhttp.h',
      ],
    },
    {
      'target_name': 'happyhttp_lib_test',
      'type': 'executable',
      'sources': [
        'src/test.cpp',
      ],
      'dependencies': [
        'happyhttp_lib',
      ],
    },
  ],
}
