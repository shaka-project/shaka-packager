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
      '../../..',
    ],
  },
  'targets': [
    {
      'target_name': 'mp4',
      'type': '<(component)',
      'sources': [
        'aac_audio_specific_config.cc',
        'aac_audio_specific_config.h',
        'box.cc',
        'box.h',
        'box_buffer.h',
        'box_definitions.cc',
        'box_definitions.h',
        'box_reader.cc',
        'box_reader.h',
        'box_writer.h',
        'cenc.cc',
        'cenc.h',
        'chunk_info_iterator.cc',
        'chunk_info_iterator.h',
        'composition_offset_iterator.cc',
        'composition_offset_iterator.h',
        'decoding_time_iterator.cc',
        'decoding_time_iterator.h',
        'es_descriptor.cc',
        'es_descriptor.h',
        'fourccs.h',
        'fragmenter.cc',
        'fragmenter.h',
        'mp4_media_parser.cc',
        'mp4_media_parser.h',
        'mp4_muxer.cc',
        'mp4_muxer.h',
        'multi_segment_segmenter.cc',
        'multi_segment_segmenter.h',
        'offset_byte_queue.cc',
        'offset_byte_queue.h',
        'rcheck.h',
        'segmenter.cc',
        'segmenter.h',
        'single_segment_segmenter.cc',
        'single_segment_segmenter.h',
        'sync_sample_iterator.cc',
        'sync_sample_iterator.h',
        'track_run_iterator.cc',
        'track_run_iterator.h',
      ],
      'dependencies': [
        '../../base/media_base.gyp:base',
      ],
    },
    {
      'target_name': 'mp4_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'aac_audio_specific_config_unittest.cc',
        'box_definitions_unittest.cc',
        'box_reader_unittest.cc',
        'chunk_info_iterator_unittest.cc',
        'composition_offset_iterator_unittest.cc',
        'decoding_time_iterator_unittest.cc',
        'es_descriptor_unittest.cc',
        'mp4_media_parser_unittest.cc',
        'offset_byte_queue_unittest.cc',
        'sync_sample_iterator_unittest.cc',
        'track_run_iterator_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../test/media_test.gyp:media_test_support',
        'mp4',
      ]
    },
  ],
}
