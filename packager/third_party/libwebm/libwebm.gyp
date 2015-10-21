# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'includes': [
  ],
  'targets': [
    {
      'target_name': 'libwebm',
      'type': 'static_library',
      'sources': [
        'src/mkvmuxer.cpp',
        'src/mkvmuxer.hpp',
        'src/mkvmuxerutil.cpp',
        'src/mkvmuxerutil.hpp',
        'src/mkvwriter.cpp',
        'src/mkvwriter.hpp',
      ],
    },
  ],
}
