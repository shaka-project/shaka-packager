# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'boringssl.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['os_posix == 1', {
        'cflags_c': [ '-std=c99' ],
        'defines': [ '_XOPEN_SOURCE=700' ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'boringssl_nacl_win64',
      'type': '<(component)',
      'sources': [
        '<@(boringssl_crypto_sources)',
      ],
      'defines': [
        'BORINGSSL_IMPLEMENTATION',
        'BORINGSSL_NO_STATIC_INITIALIZER',
        'OPENSSL_NO_ASM',
        'OPENSSL_SMALL',
      ],
      'configurations': {
        'Common_Base': {
          'msvs_target_platform': 'x64',
        },
      },
      # TODO(davidben): Fix size_t truncations in BoringSSL.
      # https://crbug.com/429039
      'msvs_disabled_warnings': [ 4267, ],
      'conditions': [
        ['component == "shared_library"', {
          'defines': [
            'BORINGSSL_SHARED_LIBRARY',
          ],
        }],
      ],
      'include_dirs': [
        'src/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include',
        ],
        'conditions': [
          ['component == "shared_library"', {
            'defines': [
              'BORINGSSL_SHARED_LIBRARY',
            ],
          }],
        ],
      },
    },
    {
      'target_name': 'boringssl',
      'type': '<(component)',
      'sources': [
        '<@(boringssl_crypto_sources)',
        '<@(boringssl_ssl_sources)',
      ],
      'defines': [
        'BORINGSSL_IMPLEMENTATION',
        'BORINGSSL_NO_STATIC_INITIALIZER',
        'OPENSSL_SMALL',
      ],
      'dependencies': [ 'boringssl_asm' ],
      # TODO(davidben): Fix size_t truncations in BoringSSL.
      # https://crbug.com/429039
      'msvs_disabled_warnings': [ 4267, ],
      'conditions': [
        ['component == "shared_library"', {
          'defines': [
            'BORINGSSL_SHARED_LIBRARY',
          ],
        }],
      ],
      'include_dirs': [
        'src/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include',
        ],
        'conditions': [
          ['component == "shared_library"', {
            'defines': [
              'BORINGSSL_SHARED_LIBRARY',
            ],
          }],
        ],
      },
    },
    {
      # boringssl_asm is a separate target to allow for ASM-specific cflags.
      'target_name': 'boringssl_asm',
      'type': 'static_library',
      'include_dirs': [
        'src/include',
      ],
      'conditions': [
        ['target_arch == "arm" and msan == 0', {
          'conditions': [
            ['OS == "linux" or OS == "android"', {
              'sources': [ '<@(boringssl_linux_arm_sources)' ],
            }, {
              'direct_dependent_settings': {
                'defines': [ 'OPENSSL_NO_ASM' ],
              },
            }],
          ],
        }],
        ['target_arch == "arm" and clang == 1', {
          # TODO(hans) Enable integrated-as (crbug.com/124610).
          'cflags': [ '-fno-integrated-as' ],
          'conditions': [
            ['OS == "android"', {
              # Else /usr/bin/as gets picked up.
              'cflags': [ '-B<(android_toolchain)' ],
            }],
          ],
        }],
        ['target_arch == "arm64" and msan == 0', {
          'conditions': [
            ['OS == "linux" or OS == "android"', {
              'sources': [ '<@(boringssl_linux_aarch64_sources)' ],
              # TODO(davidben): Remove explicit arch flag once
              # https://crbug.com/576858 is fixed.
              'cflags': [ '-march=armv8-a+crypto' ],
            }, {
              'direct_dependent_settings': {
                'defines': [ 'OPENSSL_NO_ASM' ],
              },
            }],
          ],
        }],
        ['target_arch == "ia32" and msan == 0', {
          'conditions': [
            ['OS == "mac"', {
              'sources': [ '<@(boringssl_mac_x86_sources)' ],
            }],
            ['OS == "linux" or OS == "android"', {
              'sources': [ '<@(boringssl_linux_x86_sources)' ],
            }],
            ['OS == "win"', {
              'sources': [ '<@(boringssl_win_x86_sources)' ],
              # Windows' assembly is built with Yasm. The other platforms use
              # the platform assembler.
              'variables': {
                'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/third_party/boringssl',
              },
              'includes': [
                '../yasm/yasm_compile.gypi',
              ],
            }],
            ['OS != "mac" and OS != "linux" and OS != "win" and OS != "android"', {
              'direct_dependent_settings': {
                'defines': [ 'OPENSSL_NO_ASM' ],
              },
            }],
          ]
        }],
        ['target_arch == "x64" and msan == 0', {
          'conditions': [
            ['OS == "mac"', {
              'sources': [ '<@(boringssl_mac_x86_64_sources)' ],
            }],
            ['OS == "linux" or OS == "android"', {
              'sources': [ '<@(boringssl_linux_x86_64_sources)' ],
            }],
            ['OS == "win"', {
              'sources': [ '<@(boringssl_win_x86_64_sources)' ],
              # Windows' assembly is built with Yasm. The other platforms use
              # the platform assembler.
              'variables': {
                'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/third_party/boringssl',
              },
              'includes': [
                '../yasm/yasm_compile.gypi',
              ],
            }],
            ['OS != "mac" and OS != "linux" and OS != "win" and OS != "android"', {
              'direct_dependent_settings': {
                'defines': [ 'OPENSSL_NO_ASM' ],
              },
            }],
          ]
        }],
        ['msan == 1 or (target_arch != "arm" and target_arch != "ia32" and target_arch != "x64" and target_arch != "arm64")', {
          'direct_dependent_settings': {
            'defines': [ 'OPENSSL_NO_ASM' ],
          },
        }],
      ],
    },
  ],
}
