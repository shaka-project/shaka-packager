# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'variables': {
    # Compile as chromium code to enable warnings and warnings-as-errors.
    'chromium_code': 1,
  },
  'target_defaults': {
    'include_dirs': [
      '../..',
    ],
  },
  'targets': [
    {
      'target_name': 'status',
      'type': '<(component)',
      'sources': [
        'status.cc',
        'status.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'http_fetcher',
      'type': '<(component)',
      'sources': [
        'http_fetcher.cc',
        'http_fetcher.h',
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
        '../../third_party/happyhttp/happyhttp.gyp:happyhttp_lib',
        'status',
      ],
    },
    {
      'target_name': 'base',
      'type': '<(component)',
      'sources': [
        'aes_encryptor.cc',
        'aes_encryptor.h',
        'audio_stream_info.cc',
        'audio_stream_info.h',
        'audio_timestamp_helper.cc',
        'audio_timestamp_helper.h',
        'bit_reader.cc',
        'bit_reader.h',
        'buffer_reader.cc',
        'buffer_reader.h',
        'buffer_writer.cc',
        'buffer_writer.h',
        'byte_queue.cc',
        'byte_queue.h',
        'closure_thread.cc',
        'closure_thread.h',
        'container_names.cc',
        'container_names.h',
        'demuxer.cc',
        'demuxer.h',
        'decrypt_config.cc',
        'decrypt_config.h',
        'decryptor_source.h',
        'encryption_key_source.cc',
        'encryption_key_source.h',
        'limits.h',
        'media_parser.h',
        'media_sample.cc',
        'media_sample.h',
        'media_stream.cc',
        'media_stream.h',
        'muxer.cc',
        'muxer.h',
        'muxer_options.cc',
        'muxer_options.h',
        'muxer_util.cc',
        'muxer_util.h',
        'offset_byte_queue.cc',
        'offset_byte_queue.h',
        'producer_consumer_queue.h',
        'request_signer.cc',
        'request_signer.h',
        'rsa_key.cc',
        'rsa_key.h',
        'stream_info.cc',
        'stream_info.h',
        'text_track.h',
        'timestamp.h',
        'video_stream_info.cc',
        'video_stream_info.h',
        'widevine_encryption_key_source.cc',
        'widevine_encryption_key_source.h',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/openssl/openssl.gyp:openssl',
        'http_fetcher',
        'status',
      ],
    },
    {
      'target_name': 'media_base_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'aes_encryptor_unittest.cc',
        'audio_timestamp_helper_unittest.cc',
        'bit_reader_unittest.cc',
        'buffer_writer_unittest.cc',
        'closure_thread_unittest.cc',
        'container_names_unittest.cc',
        'fake_prng.cc',  # For rsa_key_unittest
        'fake_prng.h',   # For rsa_key_unittest
        'muxer_util_unittest.cc',
        'offset_byte_queue_unittest.cc',
        'producer_consumer_queue_unittest.cc',
        'rsa_key_unittest.cc',
        'rsa_test_data.cc',  # For rsa_key_unittest
        'rsa_test_data.h',   # For rsa_key_unittest
        'status_test_util.h',
        'status_test_util_unittest.cc',
        'status_unittest.cc',
        'widevine_encryption_key_source_unittest.cc',
      ],
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        '../../testing/gmock.gyp:gmock',
        '../../third_party/openssl/openssl.gyp:openssl',
        '../file/file.gyp:file',
        '../test/media_test.gyp:media_test_support',
        'base',
      ],
    },
  ],
}
