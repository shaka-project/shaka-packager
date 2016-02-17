# Copyright 2016 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# This file contains global overrides to the chromium common.gypi settings .

{
  'target_defaults': {
    'conditions': [
      ['OS=="mac"', {
        'xcode_settings' : {
          'CLANG_CXX_LIBRARY': 'libc++', # -stdlib=libc++
        },
      }],
    ],
  },
}
