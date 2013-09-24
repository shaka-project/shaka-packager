# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'openssl',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(pkg-config --cflags openssl)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(pkg-config --libs-only-L --libs-only-other openssl)',
        ],
        'libraries': [
          '<!@(pkg-config --libs-only-l openssl)',
        ],
      },
    }
  ],
}
