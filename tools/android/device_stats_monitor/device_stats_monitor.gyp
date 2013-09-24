# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'device_stats_monitor',
      'type': 'none',
      'dependencies': [
        'device_stats_monitor_symbols',
      ],
      'actions': [
        {
          'action_name': 'strip_device_stats_monitor',
          'inputs': ['<(PRODUCT_DIR)/device_stats_monitor_symbols'],
          'outputs': ['<(PRODUCT_DIR)/device_stats_monitor'],
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
      'target_name': 'device_stats_monitor_symbols',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../../..',
      ],
      'sources': [
        'device_stats_monitor.cc',
      ],
    },
  ],
}
