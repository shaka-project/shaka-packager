# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Depend on the Android NDK's cpu feature detection. The WebView build is part
# of the system and the library already exists; for the normal build there is a
# gyp file in the checked-in NDK to build it.
{
  'conditions': [
    ['android_webview_build == 1', {
      # This is specified twice intentionally: Android provides include paths
      # to targets automatically if they depend on libraries, so we add this
      # library to every target that includes this .gypi to make the headers
      # available, then also add it to targets that link those targets via
      # link_settings to ensure it ends up being linked even if the main target
      # doesn't include this .gypi.
      'libraries': [
        'cpufeatures.a',
      ],
      'link_settings': {
        'libraries': [
          'cpufeatures.a',
        ],
      },
    }, {
      'dependencies': [
        '<(android_ndk_root)/android_tools_ndk.gyp:cpu_features',
      ],
    }],
  ],
}
