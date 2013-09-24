# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'fake_dns',
      'type': 'none',
      'dependencies': [
        'fake_dns_symbols',
      ],
      'actions': [
        {
          'action_name': 'strip_fake_dns',
          'inputs': ['<(PRODUCT_DIR)/fake_dns_symbols'],
          'outputs': ['<(PRODUCT_DIR)/fake_dns'],
          'action': [
            '<(android_strip)',
            '--strip-unneeded',
            '<@(_inputs)',
            '-o',
            '<@(_outputs)',
          ],
        },
      ],
    }, {
      'target_name': 'fake_dns_symbols',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../net/net.gyp:net',
        '../common/common.gyp:android_tools_common',
      ],
      'include_dirs': [
        '../../..',
      ],
      'sources': [
        'fake_dns.cc',
      ],
    },
  ],
}

