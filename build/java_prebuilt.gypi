# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to package prebuilt Java JARs in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my-package_java',
#   'type': 'none',
#   'variables': {
#     'jar_path': 'path/to/your.jar',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Required variables:
#  jar_path - The path to the prebuilt Java JAR file.

{
  'dependencies': [
    '<(DEPTH)/build/android/setup.gyp:build_output_dirs'
  ],
  'variables': {
    'dex_path': '<(PRODUCT_DIR)/lib.java/<(_target_name).dex.jar',
  },
  'all_dependent_settings': {
    'variables': {
      'input_jars_paths': ['<(jar_path)'],
      'library_dexed_jars_paths': ['<(dex_path)'],
    },
  },
  'actions': [
    {
      'action_name': 'dex_<(_target_name)',
      'message': 'Dexing <(_target_name) jar',
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/dex.py',
        '<(jar_path)',
      ],
      'outputs': [
        '<(dex_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/dex.py',
        '--dex-path=<(dex_path)',
        '--android-sdk-tools=<(android_sdk_tools)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo \'>(_inputs)\' | md5sum)',

        '<(jar_path)',
      ]
    },

  ],
}
