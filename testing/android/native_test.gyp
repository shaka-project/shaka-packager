# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'native_test_native_code',
          'message': 'building native pieces of native test package',
          'type': 'static_library',
          'sources': [
            'native_test_launcher.cc',
          ],
          'direct_dependent_settings': {
            'ldflags!': [
              # JNI_OnLoad is implemented in a .a and we need to
              # re-export in the .so.
              '-Wl,--exclude-libs=ALL',
            ],
          },
          'dependencies': [
            '../../base/base.gyp:base',
            '../../base/base.gyp:test_support_base',
            '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../gtest.gyp:gtest',
            'native_test_jni_headers',
            'native_test_util',
          ],
        },
        {
          'target_name': 'native_test_jni_headers',
          'type': 'none',
          'sources': [
            'java/src/org/chromium/native_test/ChromeNativeTestActivity.java'
          ],
          'variables': {
            'jni_gen_package': 'testing',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
          # So generated jni headers can be found by targets that
          # depend on this.
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
        },
        {
          'target_name': 'native_test_util',
          'type': 'static_library',
          'sources': [
            'native_test_util.cc',
            'native_test_util.h',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
          ],
        },
      ],
    }]
  ],
}
