# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
    'base.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'base_untrusted',
          'type': 'none',
          'variables': {
            'base_target': 1,
            'nacl_untrusted_build': 1,
            'nlib_target': 'libbase_untrusted.a',
            'build_glibc': 1,
            'build_newlib': 1,
            'build_irt': 1,
            'sources': [
              'base_switches.cc',
              'base_switches.h',
              'strings/string16.cc',
              'sync_socket_nacl.cc',
              'time/time_posix.cc',
            ],
          },
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
          ],
        },
      ],
    }],
  ],
}
