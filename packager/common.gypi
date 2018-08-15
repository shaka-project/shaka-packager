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
    },
    'shaka_code%': '<(shaka_code)',
    'musl%': '<(musl)',
    'libpackager_type%': 'static_library',
    'conditions': [
      ['shaka_code==1', {
        # This enable warnings and warnings-as-errors.
        'chromium_code': 1,
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
