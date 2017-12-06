# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'variables': {
    'shaka_code': 1,
  },
  'targets': [
    {
      'target_name': 'version',
      'type': '<(component)',
      'defines': [
        'PACKAGER_VERSION="<!(python generate_version_string.py)"',
      ],
      'sources': [
        'version.cc',
        'version.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
  ],
}
