# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets' : [
    {
      'target_name': 'clear_system_cache',
      'type': 'executable',
      'toolsets': ['target'],
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../base/base.gyp:test_support_base',
      ],
      'sources': [
        'clear_system_cache_main.cc',
      ],
    },
  ],
}
