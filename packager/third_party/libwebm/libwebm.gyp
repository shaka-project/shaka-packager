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
      'target_name': 'mkvmuxer',
      'type': 'static_library',
      'sources': [
        'src/common/webmids.h',
        'src/mkvmuxer/mkvmuxer.cc',
        'src/mkvmuxer/mkvmuxer.h',
        'src/mkvmuxer/mkvmuxertypes.h',
        'src/mkvmuxer/mkvmuxerutil.cc',
        'src/mkvmuxer/mkvmuxerutil.h',
        'src/mkvmuxer/mkvwriter.cc',
        'src/mkvmuxer/mkvwriter.h',
        'src/mkvmuxer.hpp'
        'src/mkvmuxerutil.hpp'
        'src/webmids.hpp'
      ],
      'include_dirs': [
        'src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src',
        ],
      },
    },
  ],
}
