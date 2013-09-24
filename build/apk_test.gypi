# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build APK based test suites.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'test_suite_name_apk',
#   'type': 'none',
#   'variables': {
#     'test_suite_name': 'test_suite_name',  # string
#     'input_shlib_path' : '/path/to/test_suite.so',  # string
#     'input_jars_paths': ['/path/to/test_suite.jar', ... ],  # list
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#

{
  'dependencies': [
    '<(DEPTH)/base/base.gyp:base_java',
    '<(DEPTH)/tools/android/android_tools.gyp:android_tools',
  ],
  'conditions': [
     ['OS == "android" and gtest_target_type == "shared_library"', {
       'variables': {
         # These are used to configure java_apk.gypi included below.
         'apk_name': '<(test_suite_name)',
         'intermediate_dir': '<(PRODUCT_DIR)/<(test_suite_name)_apk',
         'final_apk_path': '<(intermediate_dir)/<(test_suite_name)-debug.apk',
         'java_in_dir': '<(DEPTH)/testing/android/java',
         'android_manifest_path': '<(DEPTH)/testing/android/AndroidManifest.xml',
         'native_lib_target': 'lib<(test_suite_name)',
         # TODO(yfriedman, cjhopman): Support managed installs for gtests.
         'gyp_managed_install': 0,
       },
       'includes': [ 'java_apk.gypi' ],
     }],  # 'OS == "android" and gtest_target_type == "shared_library"
  ],  # conditions
}
