# Copyright 2014 Google Inc. All rights reserved.
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
      'target_name': 'libpackager',
      'type': '<(libpackager_type)',
      'sources': [
        # TODO(kqyang): Clean up the file path.
        'app/job_manager.cc',
        'app/job_manager.h',
        'app/muxer_factory.cc',
        'app/muxer_factory.h',
        'app/libcrypto_threading.cc',
        'app/libcrypto_threading.h',
        'app/packager_util.cc',
        'app/packager_util.h',
        'packager.cc',
        'packager.h',
      ],
      'dependencies': [
        'file/file.gyp:file',
        'hls/hls.gyp:hls_builder',
        'media/chunking/chunking.gyp:chunking',
        'media/codecs/codecs.gyp:codecs',
        'media/crypto/crypto.gyp:crypto',
        'media/demuxer/demuxer.gyp:demuxer',
        'media/event/media_event.gyp:media_event',
        'media/formats/mp2t/mp2t.gyp:mp2t',
        'media/formats/mp4/mp4.gyp:mp4',
        'media/formats/packed_audio/packed_audio.gyp:packed_audio',
        'media/formats/webm/webm.gyp:webm',
        'media/formats/webvtt/webvtt.gyp:webvtt',
        'media/formats/wvm/wvm.gyp:wvm',
        'media/public/public.gyp:public',
        'media/replicator/replicator.gyp:replicator',
        'media/trick_play/trick_play.gyp:trick_play',
        'mpd/mpd.gyp:mpd_builder',
        'third_party/boringssl/boringssl.gyp:boringssl',
        'version/version.gyp:version',
      ],
      'conditions': [
        ['libpackager_type == "shared_library"', {
          'defines': [
            'SHARED_LIBRARY_BUILD',
            'SHAKA_IMPLEMENTATION',
          ],
        }],
        ['libpackager_type == "static_library"', {
          'standalone_static_library': 1,
        }],
      ],
    },
    {
      'target_name': 'packager',
      'type': 'executable',
      'sources': [
        'app/ad_cue_generator_flags.cc',
        'app/ad_cue_generator_flags.h',
        'app/crypto_flags.cc',
        'app/crypto_flags.h',
        'app/gflags_hex_bytes.cc',
        'app/gflags_hex_bytes.h',
        'app/hls_flags.cc',
        'app/hls_flags.h',
        'app/manifest_flags.cc',
        'app/manifest_flags.h',
        'app/mpd_flags.cc',
        'app/mpd_flags.h',
        'app/muxer_flags.cc',
        'app/muxer_flags.h',
        'app/packager_main.cc',
        'app/playready_key_encryption_flags.cc',
        'app/playready_key_encryption_flags.h',
        'app/raw_key_encryption_flags.cc',
        'app/raw_key_encryption_flags.h',
        'app/protection_system_flags.cc',
        'app/protection_system_flags.h',
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
        'file/file.gyp:file',
        'libpackager',
        'third_party/gflags/gflags.gyp:gflags',
        'tools/license_notice.gyp:license_notice',
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
        'tools/license_notice.gyp:license_notice',
      ],
    },
    {
      'target_name': 'packager_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'packager_test.cc',
      ],
      'dependencies': [
        'libpackager',
        'testing/gmock.gyp:gmock',
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
      'target_name': 'pssh_box_py',
      'type': 'none',
      'copies': [{
        'destination': '<(PRODUCT_DIR)',
        'files': [
          'tools/pssh/pssh-box.py',
        ],
      }],
      'dependencies': [
        'media/base/media_base.gyp:widevine_pssh_data_proto',
        'third_party/protobuf/protobuf.gyp:py_proto',
      ],
    },
    {
      'target_name': 'status',
      'type': '<(component)',
      'sources': [
        'status.cc',
        'status.h',
        'status_macros.h',
      ],
      'dependencies': [
        'base/base.gyp:base',
      ],
      'conditions': [
        ['libpackager_type == "shared_library"', {
          'defines': [
            'SHARED_LIBRARY_BUILD',
            'SHAKA_IMPLEMENTATION',
          ],
        }],
      ],
    },
    {
      'target_name': 'status_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'status_unittest.cc',
      ],
      'dependencies': [
        'status',
        'testing/gmock.gyp:gmock',
        'testing/gtest.gyp:gtest',
        'testing/gtest.gyp:gtest_main',
      ]
    },
    {
      'target_name': 'packager_builder_tests',
      'type': 'none',
      'dependencies': [
        'file/file.gyp:file_unittest',
        'hls/hls.gyp:hls_unittest',
        'media/base/media_base.gyp:media_base_unittest',
        'media/chunking/chunking.gyp:chunking_unittest',
        'media/codecs/codecs.gyp:codecs_unittest',
        'media/crypto/crypto.gyp:crypto_unittest',
        'media/demuxer/demuxer.gyp:demuxer_unittest',
        'media/event/media_event.gyp:media_event_unittest',
        'media/formats/mp2t/mp2t.gyp:mp2t_unittest',
        'media/formats/mp4/mp4.gyp:mp4_unittest',
        'media/formats/packed_audio/packed_audio.gyp:packed_audio_unittest',
        'media/formats/webm/webm.gyp:webm_unittest',
        'media/formats/webvtt/webvtt.gyp:webvtt_unittest',
        'media/formats/wvm/wvm.gyp:wvm_unittest',
        'media/trick_play/trick_play.gyp:trick_play_unittest',
        'mpd/mpd.gyp:mpd_unittest',
        'packager_test',
        'status_unittest',
      ],
    },
  ],
}
