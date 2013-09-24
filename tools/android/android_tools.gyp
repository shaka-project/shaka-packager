# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # Intermediate target grouping the android tools needed to run native
    # unittests and instrumentation test apks.
    {
      'target_name': 'android_tools',
      'type': 'none',
      'dependencies': [
        'fake_dns/fake_dns.gyp:fake_dns',
        'forwarder2/forwarder.gyp:forwarder2',
        'md5sum/md5sum.gyp:md5sum',
        'adb_reboot/adb_reboot.gyp:adb_reboot',
      ],
    },
    {
      'target_name': 'memdump',
      'type': 'none',
      'dependencies': [
        'memdump/memdump.gyp:memdump',
      ],
    }
  ],
}
