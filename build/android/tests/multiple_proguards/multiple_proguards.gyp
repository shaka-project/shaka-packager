# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
    'package_name': 'multiple_proguard',
  },
  'targets': [
    {
      'target_name': 'multiple_proguards_test_apk',
      'type': 'none',
      'variables': {
        'app_manifest_version_name%': '<(android_app_version_name)',
        'java_in_dir': '.',
        'proguard_enabled': 'true',
        'proguard_flags_paths': [
          'proguard1.flags',
          'proguard2.flags',
        ],
        'R_package': 'dummy',
        'R_package_relpath': 'dummy',
        'apk_name': 'MultipleProguards',
        # This is a build-only test. There's nothing to install.
        'gyp_managed_install': 0,
      },
      'dependencies': [
        # guava has references to objects using reflection which
        # should be ignored in proguard step.
        '../../../../third_party/guava/guava.gyp:guava_javalib',
      ],
      'includes': [ '../../../../build/java_apk.gypi' ],
    },
  ],
}
