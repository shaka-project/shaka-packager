# Copyright 2017 Google Inc. All rights reserved.
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
      'target_name': 'replicator',
      'type': '<(component)',
      'sources': [
        'replicator.cc',
        'replicator.h',
      ],
      'dependencies': [
        '../base/media_base.gyp:media_base',
      ],
    },
  ],
}
