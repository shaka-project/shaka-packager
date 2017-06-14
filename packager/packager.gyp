# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'libpackager',
      'type': '<(libpackager_type)',
      'sources': [
        'packager.cc',
        'packager.h',
        # TODO(kqyang): Clean up the file path.
        'app/libcrypto_threading.cc',
        'app/libcrypto_threading.h',
        'app/packager_util.cc',
        'app/packager_util.h',
      ],
      'dependencies': [
        'hls/hls.gyp:hls_builder',
        'media/codecs/codecs.gyp:codecs',
        'media/chunking/chunking.gyp:chunking',
        'media/demuxer/demuxer.gyp:demuxer',
        'media/event/media_event.gyp:media_event',
        'media/file/file.gyp:file',
        'media/formats/mp2t/mp2t.gyp:mp2t',
        'media/formats/mp4/mp4.gyp:mp4',
        'media/formats/mpeg/mpeg.gyp:mpeg',
        'media/formats/webm/webm.gyp:webm',
        'media/formats/webvtt/webvtt.gyp:webvtt',
        'media/formats/wvm/wvm.gyp:wvm',
        'media/trick_play/trick_play.gyp:trick_play',
        'mpd/mpd.gyp:mpd_builder',
        'third_party/boringssl/boringssl.gyp:boringssl',
        'version/version.gyp:version',
      ],
      'conditions': [
        ['libpackager_type=="shared_library"', {
          'defines': [
            'SHARED_LIBRARY_BUILD',
            'SHAKA_IMPLEMENTATION',
          ],
        }],
      ],
    },
    {
      'target_name': 'packager',
      'type': 'executable',
      'sources': [
        'app/crypto_flags.cc',
        'app/crypto_flags.h',
        'app/fixed_key_encryption_flags.cc',
        'app/fixed_key_encryption_flags.h',
        'app/gflags_hex_bytes.cc',
        'app/gflags_hex_bytes.h',
        'app/hls_flags.cc',
        'app/hls_flags.h',
        'app/mpd_flags.cc',
        'app/mpd_flags.h',
        'app/muxer_flags.cc',
        'app/muxer_flags.h',
        'app/packager_main.cc',
        'app/playready_key_encryption_flags.cc',
        'app/playready_key_encryption_flags.h',
        'app/retired_flags.cc',
        'app/retired_flags.h',
        'app/stream_descriptor.cc',
        'app/stream_descriptor.h',
        'app/validate_flag.cc',
        'app/validate_flag.h',
        'app/vlog_flags.cc',
        'app/vlog_flags.h',
        'app/widevine_encryption_flags.cc',
        'app/widevine_encryption_flags.h',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'libpackager',
        'media/file/file.gyp:file',
        'third_party/gflags/gflags.gyp:gflags',
      ],
      'conditions': [
        ['profiling==1', {
          'dependencies': [
            'base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
    },
    {
      'target_name': 'mpd_generator',
      'type': 'executable',
      'sources': [
        'app/mpd_generator.cc',
        'app/mpd_generator_flags.h',
        'app/vlog_flags.cc',
        'app/vlog_flags.h',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'mpd/mpd.gyp:mpd_util',
        'third_party/gflags/gflags.gyp:gflags',
      ],
    },
    {
      'target_name': 'packager_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'packager_test.cc',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'libpackager',
        'testing/gtest.gyp:gtest',
        'testing/gtest.gyp:gtest_main',
      ],
    },
    {
      'target_name': 'packager_test_py_copy',
      'type': 'none',
      'copies': [{
        'destination': '<(PRODUCT_DIR)',
        'files': [
          'app/test/packager_app.py',
          'app/test/packager_test.py',
          'app/test/test_env.py',
        ],
      }],
      'dependencies': ['packager'],
    },
    {
      'target_name': 'packager_builder_tests',
      'type': 'none',
      'dependencies': [
        'hls/hls.gyp:hls_unittest',
        'media/base/media_base.gyp:media_base_unittest',
        'media/chunking/chunking.gyp:chunking_unittest',
        'media/codecs/codecs.gyp:codecs_unittest',
        'media/crypto/crypto.gyp:crypto_unittest',
        'media/demuxer/demuxer.gyp:demuxer_unittest',
        'media/event/media_event.gyp:media_event_unittest',
        'media/file/file.gyp:file_unittest',
        'media/formats/mp2t/mp2t.gyp:mp2t_unittest',
        'media/formats/mp4/mp4.gyp:mp4_unittest',
        'media/formats/webm/webm.gyp:webm_unittest',
        'media/formats/webvtt/webvtt.gyp:webvtt_unittest',
        'media/formats/wvm/wvm.gyp:wvm_unittest',
        'media/trick_play/trick_play.gyp:trick_play_unittest',
        'mpd/mpd.gyp:mpd_unittest',
        'packager_test',
      ],
    },
  ],
}
