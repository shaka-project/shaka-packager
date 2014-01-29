# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# TODO(kqyang): this file should be in media directory.
{
  'target_defaults': {
    'include_dirs': [
      '.',
    ],
  },
  'targets': [
    {
      'target_name': 'httpfetcher',
      'type': 'static_library',
      'sources': [
        'media/base/httpfetcher.cc',
        'media/base/httpfetcher.h',
        'media/base/status.cc',
      ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          }
        }]
      ],
      'dependencies': [
        'third_party/happyhttp/happyhttp.gyp:happyhttp_lib',
      ],
    },
    {
      # Note that this test performs real http requests to a http server.
      'target_name': 'httpfetcher_unittest',
      'type': 'executable',
      'sources': [
        'media/base/httpfetcher_unittest.cc',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'httpfetcher',
        'testing/gtest.gyp:gtest',
        'testing/gtest.gyp:gtest_main',
      ],
    },
    {
      'target_name': 'media_base',
      'type': 'static_library',
      'sources': [
        'media/base/aes_encryptor.cc',
        'media/base/aes_encryptor.h',
        'media/base/audio_stream_info.cc',
        'media/base/audio_stream_info.h',
        'media/base/bit_reader.cc',
        'media/base/bit_reader.h',
        'media/base/buffers.h',
        'media/base/buffer_reader.cc',
        'media/base/buffer_reader.h',
        'media/base/buffer_writer.cc',
        'media/base/buffer_writer.h',
        'media/base/byte_queue.cc',
        'media/base/byte_queue.h',
        'media/base/container_names.cc',
        'media/base/container_names.h',
        # TODO(kqyang): demuxer should not be here, it looks like some kinds of
        # circular dependencies.
        'media/base/demuxer.cc',
        'media/base/demuxer.h',
        'media/base/decrypt_config.cc',
        'media/base/decrypt_config.h',
        'media/base/decryptor_source.h',
        'media/base/encryptor_source.cc',
        'media/base/encryptor_source.h',
        'media/base/fixed_encryptor_source.cc',
        'media/base/fixed_encryptor_source.h',
        'media/base/limits.h',
        'media/base/media_parser.h',
        'media/base/media_sample.cc',
        'media/base/media_sample.h',
        'media/base/media_stream.cc',
        'media/base/media_stream.h',
        'media/base/muxer.cc',
        'media/base/muxer.h',
        'media/base/muxer_options.cc',
        'media/base/muxer_options.h',
        'media/base/request_signer.cc',
        'media/base/request_signer.h',
        'media/base/rsa_key.cc',
        'media/base/rsa_key.h',
        'media/base/status.cc',
        'media/base/status.h',
        'media/base/stream_info.cc',
        'media/base/stream_info.h',
        'media/base/text_track.h',
        'media/base/video_stream_info.cc',
        'media/base/video_stream_info.h',
        'media/base/widevine_encryptor_source.cc',
        'media/base/widevine_encryptor_source.h',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'httpfetcher',
        'third_party/openssl/openssl.gyp:openssl',
      ],
    },
    {
      'target_name': 'media_test_support',
      'type': 'static_library',
      'sources': [
        'media/test/run_tests_with_atexit_manager.cc',
        'media/test/test_data_util.cc',
        'media/test/test_data_util.h',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'media_base_unittest',
      'type': 'executable',
      'sources': [
        'media/base/aes_encryptor_unittest.cc',
        'media/base/bit_reader_unittest.cc',
        'media/base/buffer_writer_unittest.cc',
        'media/base/container_names_unittest.cc',
        'media/base/fake_prng.cc',  # For rsa_key_unittest
        'media/base/fake_prng.h',   # For rsa_key_unittest
        'media/base/rsa_key_unittest.cc',
        'media/base/rsa_test_data.cc',  # For rsa_key_unittest
        'media/base/rsa_test_data.h',   # For rsa_key_unittest
        'media/base/status_test_util.h',
        'media/base/status_test_util_unittest.cc',
        'media/base/status_unittest.cc',
      ],
      'dependencies': [
        'file',
        'media_base',
        'media_test_support',
        'testing/gtest.gyp:gtest',
        'testing/gmock.gyp:gmock',
      ],
    },
    {
      'target_name': 'mp4',
      'type': 'static_library',
      'sources': [
        'media/mp4/aac_audio_specific_config.cc',
        'media/mp4/aac_audio_specific_config.h',
        'media/mp4/box.cc',
        'media/mp4/box.h',
        'media/mp4/box_buffer.h',
        'media/mp4/box_definitions.cc',
        'media/mp4/box_definitions.h',
        'media/mp4/box_reader.cc',
        'media/mp4/box_reader.h',
        'media/mp4/box_writer.h',
        'media/mp4/cenc.cc',
        'media/mp4/cenc.h',
        'media/mp4/chunk_info_iterator.cc',
        'media/mp4/chunk_info_iterator.h',
        'media/mp4/composition_offset_iterator.cc',
        'media/mp4/composition_offset_iterator.h',
        'media/mp4/decoding_time_iterator.cc',
        'media/mp4/decoding_time_iterator.h',
        'media/mp4/es_descriptor.cc',
        'media/mp4/es_descriptor.h',
        'media/mp4/fourccs.h',
        'media/mp4/mp4_fragmenter.cc',
        'media/mp4/mp4_fragmenter.h',
        'media/mp4/mp4_general_segmenter.cc',
        'media/mp4/mp4_general_segmenter.h',
        'media/mp4/mp4_media_parser.cc',
        'media/mp4/mp4_media_parser.h',
        'media/mp4/mp4_muxer.cc',
        'media/mp4/mp4_muxer.h',
        'media/mp4/mp4_segmenter.cc',
        'media/mp4/mp4_segmenter.h',
        'media/mp4/mp4_vod_segmenter.cc',
        'media/mp4/mp4_vod_segmenter.h',
        'media/mp4/offset_byte_queue.cc',
        'media/mp4/offset_byte_queue.h',
        'media/mp4/rcheck.h',
        'media/mp4/sync_sample_iterator.cc',
        'media/mp4/sync_sample_iterator.h',
        'media/mp4/track_run_iterator.cc',
        'media/mp4/track_run_iterator.h',
      ],
      'dependencies': [
        'media_base',
      ],
    },
    {
      'target_name': 'mp4_unittest',
      'type': 'executable',
      'sources': [
        'media/mp4/aac_audio_specific_config_unittest.cc',
        'media/mp4/box_definitions_unittest.cc',
        'media/mp4/box_reader_unittest.cc',
        'media/mp4/chunk_info_iterator_unittest.cc',
        'media/mp4/composition_offset_iterator_unittest.cc',
        'media/mp4/decoding_time_iterator_unittest.cc',
        'media/mp4/es_descriptor_unittest.cc',
        'media/mp4/mp4_media_parser_unittest.cc',
        'media/mp4/offset_byte_queue_unittest.cc',
        'media/mp4/sync_sample_iterator_unittest.cc',
        'media/mp4/track_run_iterator_unittest.cc',
      ],
      'dependencies': [
        'media_test_support',
        'mp4',
        'testing/gtest.gyp:gtest',
        'testing/gmock.gyp:gmock',
      ]
    },
    {
      'target_name': 'file',
      'type': 'static_library',
      'sources': [
        'media/file/file.cc',
        'media/file/file.h',
        'media/file/file_closer.h',
        'media/file/local_file.cc',
        'media/file/local_file.h',
      ],
      'dependencies': [
        'base/base.gyp:base',
      ],
    },
    {
      'target_name': 'file_unittest',
      'type': 'executable',
      'sources': [
        'media/file/file_unittest.cc',
      ],
      'dependencies': [
        'file',
        'testing/gtest.gyp:gtest',
        'testing/gtest.gyp:gtest_main',
      ],
    },
    {
      'target_name': 'packager_test',
      'type': 'executable',
      'sources': [
        'media/test/packager_test.cc',
      ],
      'dependencies': [
        'file',
        'media_test_support',
        'mp4',
        'testing/gtest.gyp:gtest',
      ],
    },
    {
      'target_name': 'media_event',
      'type': '<(component)',
      'sources': [
        'media/event/muxer_listener.h',
        'media/event/vod_media_info_dump_muxer_listener.cc',
        'media/event/vod_media_info_dump_muxer_listener.h',
        'media/event/vod_mpd_notify_muxer_listener.cc',
        'media/event/vod_mpd_notify_muxer_listener.h',
        'media/event/vod_muxer_listener_internal.cc',
        'media/event/vod_muxer_listener_internal.h',
      ],
      'dependencies': [
        'media_base',
        'mpd/mpd.gyp:media_info_proto',
        # Depends on full protobuf to read/write with TextFormat.
        'third_party/protobuf/protobuf.gyp:protobuf_full_do_not_use',
      ],
    },
    {
      'target_name': 'media_event_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'media/event/vod_media_info_dump_muxer_listener_unittest.cc',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'base/base.gyp:run_all_unittests',
        'file',
        'media_event',
        'mpd/mpd.gyp:media_info_proto',
        'testing/gtest.gyp:gtest',
        # Depends on full protobuf to read/write with TextFormat.
        'third_party/protobuf/protobuf.gyp:protobuf_full_do_not_use',
      ],
    },
    {
      'target_name': 'packager_main',
      'type': 'executable',
      'sources': [
        'app/packager_main.cc',
      ],
      'dependencies': [
        'file',
        'media_event',
        'mp4',
        'third_party/gflags/gflags.gyp:gflags',
      ],
    },
    {
      'target_name': 'mpd_generator',
      'type': 'executable',
      'sources': [
        'app/mpd_generator.cc',
        'app/mpd_generator_flags.h',
      ],
      'dependencies': [
        'base/base.gyp:base',
        'mpd/mpd.gyp:mpd_util',
        'third_party/gflags/gflags.gyp:gflags',
      ],
    },
  ],
}
