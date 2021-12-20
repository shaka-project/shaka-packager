# Copyright 2020 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'targets': [
    {
      'target_name': 'libpng',
      'type': 'static_library',
      'sources': [
        'src/png.c',
        'src/pngerror.c',
        'src/pngget.c',
        'src/pngmem.c',
        'src/pngpread.c',
        'src/pngread.c',
        'src/pngrio.c',
        'src/pngrtran.c',
        'src/pngrutil.c',
        'src/pngset.c',
        'src/pngtrans.c',
        'src/pngwio.c',
        'src/pngwrite.c',
        'src/pngwtran.c',
        'src/pngwutil.c',
      ],
      'include_dirs': [
        '.',
        'src',
      ],
      'dependencies': [
        '../zlib/zlib.gyp:zlib',
      ],
      # Disable all CPU-specific optimizations, which may fail to link because
      # we don't have a CPU-specific set of source files.
      'defines': [
        'PNG_ARM_NEON_OPT=0',
        'PNG_INTEL_SSE_OPT=0',
        'PNG_MIPS_MSA_OPT=0',
        'PNG_POWERPC_VSX_OPT=0',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
          'src',
        ],
      },
    },
  ],
}
