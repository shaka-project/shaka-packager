# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['use_system_nspr==1', {
      'targets': [
        {
          'target_name': 'nspr',
          'type': 'none',
          'toolsets': ['host', 'target'],
          'variables': {
            'headers_root_path': '.',
            'header_filenames': [
              'prcpucfg.h',
              'prtime.h',
              'prtypes.h',
            ],
          },
          'includes': [
            '../../../build/shim_headers.gypi',
          ],
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags nspr)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other nspr)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l nspr)',
            ],
          },
        }
      ],
    }],
  ],
}
