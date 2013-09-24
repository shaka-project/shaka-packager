# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    'target_name': 'api_gen_util',
    'type': 'static_library',
    'sources': [
        'util.cc',
    ],
    'dependencies': ['<(DEPTH)/base/base.gyp:base'],
    'include_dirs': [
      '<(DEPTH)',
    ],
  }],
}
