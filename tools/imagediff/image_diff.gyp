# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets' : [
    {
      'target_name': 'image_diff',
      'type': 'executable',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/libpng/libpng.gyp:libpng',
        '../../third_party/zlib/zlib.gyp:zlib',
      ],
      'include_dirs': [
        '../../',
      ],
      'sources': [
        'image_diff.cc',
        'image_diff_png.h',
        'image_diff_png.cc',
      ],
      'conditions': [
       ['OS=="android" and android_webview_build==0', {
         # The Chromium Android port will compare images on host rather
         # than target (a device or emulator) for performance reasons.
         'toolsets': ['host'],
       }],
       ['OS=="android" and android_webview_build==1', {
         'type': 'none',
       }],
      ],
    },
  ],
}
