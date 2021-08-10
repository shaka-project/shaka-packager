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
      # This is a flag from build/common.gypi to allow linker warnings.
      # This may be necessary with static_link_binaries=1.
      'disable_fatal_linker_warnings%': '0',
      'libpackager_type%': 'static_library',
      'static_link_binaries%': '0',
    },

    'shaka_code%': '<(shaka_code)',
    'musl%': '<(musl)',
    'disable_fatal_linker_warnings%': '<(disable_fatal_linker_warnings)',
    'libpackager_type%': '<(libpackager_type)',
    'static_link_binaries%': '<(static_link_binaries)',

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
    'defines': [
      # These defines make the contents of base/mac/foundation_util.h compile
      # against the standard OSX SDK, by renaming Chrome's opaque type and
      # using the real OSX type.  This was not necessary before we switched
      # away from using hermetic copies of clang and the sysroot to build.
      'OpaqueSecTrustRef=__SecACL',
      'OpaqueSecTrustedApplicationRef=__SecTrustedApplication',
    ],
    'conditions': [
      ['shaka_code==1', {
        'include_dirs': [
          '.',
          '..',
        ],
        'cflags': [
          # This is triggered by logging macros.
          '-Wno-implicit-fallthrough',
          # Triggered by unit tests, which override things in mocks.  gmock
          # doesn't mark them as override.  An upgrade may help.  TODO: try
          # upgrading gmock.
          '-Wno-inconsistent-missing-override',
          # Triggered by base/time/time.h when using clang, but NOT on Mac.
          '-Wno-implicit-const-int-float-conversion',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS': [
            # This is triggered by logging macros.
            '-Wno-implicit-fallthrough',
            # Triggered by unit tests, which override things in mocks.  gmock
            # doesn't mark them as override.  An upgrade may help.  TODO: try
            # upgrading gmock.
            '-Wno-inconsistent-missing-override',
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
        'cflags': [
          '-Wno-error',
        ],
        'variables': {
          'clang_warning_flags': [
            '-Wno-error',
          ],
        },
        'xcode_settings': {
          'WARNING_CFLAGS': [
            '-Wno-error',
          ],
        },
        'msvs_disabled_warnings': [
          4819,  # The file contains a character that cannot be represented in
                 # the current code page. It typically happens when compiling
                 # the code in CJK environment if there is non-ASCII characters
                 # in the file.
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
      }],
      ['static_link_binaries==1', {
        'conditions': [
          ['OS=="linux"', {
            'defines': [
              # Even when we are not using musl or uClibc, pretending to use
              # uClibc on Linux is the only way to disable certain Chromium
              # base features, such as hooking into malloc.  Hooking into
              # malloc, in turn, fails when we are linking statically.
              '__UCLIBC__',
            ],
          }],
        ],
        'ldflags': [
          '-static',
        ],
      }],
    ],
  },
}
