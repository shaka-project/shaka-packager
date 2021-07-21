# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# This file contains common settings for building packager components.

{
  'variables': {
    'variables': {
      'shaka_code%': 0,
      # musl is a lightweight C standard library used in Alpine Linux.
      'musl%': 0,
      'libpackager_type%': 'static_library',
    },

    'shaka_code%': '<(shaka_code)',
    'musl%': '<(musl)',
    'libpackager_type%': '<(libpackager_type)',

    'conditions': [
      ['shaka_code==1', {
        # This enable warnings and warnings-as-errors.
        'chromium_code': 1,
      }],
      # These are some Chromium build settings that are normally keyed off of
      # component=="shared_library".  We don't use component=="shared_library"
      # because it would result in a shared lib for every single component, but
      # we still need these settings for a shared library build of libpackager
      # on Windows.
      ['libpackager_type=="shared_library"', {
        # Make sure we use a dynamic CRT to avoid issues with std::string in
        # the library API on Windows.
        'win_release_RuntimeLibrary': '2', # 2 = /MD (nondebug DLL)
        'win_debug_RuntimeLibrary': '3',   # 3 = /MDd (debug DLL)
        # Skip the Windows allocator shim on Windows.  Using this with a shared
        # library results in build errors.
        'win_use_allocator_shim': 0,
      }],
    ],
  },
  'target_defaults': {
    'conditions': [
      ['shaka_code==1', {
        'include_dirs': [
          '.',
          '..',
        ],
        'variables': {
          'clang_warning_flags': [
            '-Wimplicit-fallthrough',
          ],
          # Revert the relevant settings in Chromium's common.gypi.
          'clang_warning_flags_unset': [
            '-Wno-char-subscripts',
            '-Wno-unneeded-internal-declaration',
            '-Wno-covered-switch-default',

            # C++11-related flags:
            '-Wno-c++11-narrowing',
            '-Wno-reserved-user-defined-literal',
          ],
        },
        # TODO(kqyang): Fix these msvs warnings.
        'msvs_disabled_warnings': [
          4125,  # Decimal digit terminates octal escape sequence, e.g. "\709".
          4819,  # The file contains a character that cannot be represented in
                 # the current code page. It typically happens when compiling
                 # the code in CJK environment if there is non-ASCII characters
                 # in the file.
          4251,  # Warnings about private std::string in Status in a shared
                 # library config on Windows.
        ],
      }, {
        # We do not have control over non-shaka code. Disable some warnings to
        # make build pass.
        'variables': {
          'clang_warning_flags': [
            '-Wno-tautological-constant-compare',
            '-Wno-unguarded-availability',
          ],
        },
        'msvs_disabled_warnings': [
          4819,  # The file contains a character that cannot be represented in
                 # the current code page. It typically happens when compiling
                 # the code in CJK environment if there is non-ASCII characters
                 # in the file.
        ],
        'cflags': [
          # TODO(modmaker): Remove once Chromium base is removed.
          '-Wno-deprecated-declarations',
        ],
        'conditions': [
          ['clang==0', {
            'cflags': [
              '-Wno-dangling-else',
              '-Wno-deprecated-declarations',
              '-Wno-unused-function',
            ],
          }],
        ],
      }],
      ['musl==1', {
        'defines': [
          # musl is not uClibc but is similar to uClibc that a minimal feature
          # set is supported. One of Shaka Packager's dependencies, Chromium
          # base uses __UCLIBC__ flag to disable some features, which needs to
          # be disabled for musl too.
          '__UCLIBC__',
        ],
        'cflags!': [
          # Do not treat warnings as errors on musl as there is a hard-coded
          # warning in musl's sys/errno.h.
          '-Werror',
        ],
      }]
    ],
  },
}
