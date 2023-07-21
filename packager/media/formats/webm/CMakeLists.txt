# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(formats_webm STATIC
    encryptor.cc
    mkv_writer.cc
    multi_segment_segmenter.cc
    seek_head.cc
    segmenter.cc
    single_segment_segmenter.cc
    two_pass_single_segment_segmenter.cc
    webm_audio_client.cc
    webm_cluster_parser.cc
    webm_constants.cc
    webm_content_encodings.cc
    webm_content_encodings_client.cc
    webm_crypto_helpers.cc
    webm_info_parser.cc
    webm_parser.cc
    webm_media_parser.cc
    webm_muxer.cc
    webm_tracks_parser.cc
    webm_video_client.cc
    webm_webvtt_parser.cc
)
target_link_libraries(formats_webm
    webm
    file
    media_base
    media_codecs
)

add_executable(webm_unittest
    cluster_builder.cc
    encrypted_segmenter_unittest.cc
    encryptor_unittest.cc
    multi_segment_segmenter_unittest.cc
    segmenter_test_base.cc
    single_segment_segmenter_unittest.cc
    tracks_builder.cc
    webm_cluster_parser_unittest.cc
    webm_content_encodings_client_unittest.cc
    webm_parser_unittest.cc
    webm_tracks_parser_unittest.cc
    webm_webvtt_parser_unittest.cc
)
target_link_libraries(webm_unittest
    formats_webm
    gmock
    gtest
    gtest_main
    test_data_util
)
add_gtest(webm_unittest)
