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
      'target_name': 'mp4',
      'type': '<(component)',
      'sources': [
        'box.cc',
        'box.h',
        'box_buffer.h',
        'box_definitions.cc',
        'box_definitions.h',
        'box_reader.cc',
        'box_reader.h',
        'chunk_info_iterator.cc',
        'chunk_info_iterator.h',
        'composition_offset_iterator.cc',
        'composition_offset_iterator.h',
        'decoding_time_iterator.cc',
        'decoding_time_iterator.h',
        'fragmenter.cc',
        'fragmenter.h',
        'key_frame_info.h',
        'mp4_media_parser.cc',
        'mp4_media_parser.h',
        'mp4_muxer.cc',
        'mp4_muxer.h',
        'multi_segment_segmenter.cc',
        'multi_segment_segmenter.h',
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
        '../../../third_party/boringssl/boringssl.gyp:boringssl',
        '../../../third_party/gflags/gflags.gyp:gflags',
        '../../base/media_base.gyp:media_base',
        '../../codecs/codecs.gyp:codecs',
        '../../event/media_event.gyp:media_event',
      ],
    },
    {
      'target_name': 'mp4_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'box_definitions_unittest.cc',
        'box_reader_unittest.cc',
        'chunk_info_iterator_unittest.cc',
        'composition_offset_iterator_unittest.cc',
        'decoding_time_iterator_unittest.cc',
        'mp4_media_parser_unittest.cc',
        'sync_sample_iterator_unittest.cc',
        'track_run_iterator_unittest.cc',
      ],
      'dependencies': [
        '../../../file/file.gyp:file',
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../../third_party/gflags/gflags.gyp:gflags',
        '../../test/media_test.gyp:media_test_support',
        'mp4',
      ]
    },
  ],
}
