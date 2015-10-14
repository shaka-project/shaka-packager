# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'includes': [
    '../../../common.gypi',
  ],
  'targets': [
    {
      'target_name': 'webm',
      'type': '<(component)',
      'sources': [
        'webm_audio_client.cc',
        'webm_audio_client.h',
        'webm_cluster_parser.cc',
        'webm_cluster_parser.h',
        'webm_constants.cc',
        'webm_constants.h',
        'webm_content_encodings.cc',
        'webm_content_encodings.h',
        'webm_content_encodings_client.cc',
        'webm_content_encodings_client.h',
        'webm_crypto_helpers.cc',
        'webm_crypto_helpers.h',
        'webm_info_parser.cc',
        'webm_info_parser.h',
        'webm_parser.cc',
        'webm_parser.h',
        'webm_stream_parser.cc',
        'webm_stream_parser.h',
        'webm_tracks_parser.cc',
        'webm_tracks_parser.h',
        'webm_video_client.cc',
        'webm_video_client.h',
        'webm_webvtt_parser.cc',
        'webm_webvtt_parser.h'
      ],
      'dependencies': [
        '../../base/media_base.gyp:base',
      ],
    },
    {
      'target_name': 'webm_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'cluster_builder.cc',
        'cluster_builder.h',
        'opus_packet_builder.cc',
        'opus_packet_builder.h',
        'tracks_builder.cc',
        'tracks_builder.h',
        'webm_cluster_parser_unittest.cc',
        'webm_content_encodings_client_unittest.cc',
        'webm_parser_unittest.cc',
        'webm_tracks_parser_unittest.cc',
        'webm_webvtt_parser_unittest.cc',
      ],
      'dependencies': [
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gmock.gyp:gmock',
        '../../test/media_test.gyp:media_test_support',
        'webm',
      ]
    },
  ],
}
