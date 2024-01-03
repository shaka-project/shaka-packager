# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(media_event STATIC
    combined_muxer_listener.cc
    hls_notify_muxer_listener.cc
    mpd_notify_muxer_listener.cc
    multi_codec_muxer_listener.cc
    muxer_listener_factory.cc
    muxer_listener_internal.cc
    vod_media_info_dump_muxer_listener.cc
)
target_link_libraries(media_event
    file
    mpd_media_info_proto
    media_base
    media_codecs
)

add_library(mock_muxer_listener STATIC
    mock_muxer_listener.cc
)
target_link_libraries(mock_muxer_listener
    gmock
    media_event
)

add_executable(media_event_unittest
    hls_notify_muxer_listener_unittest.cc
    muxer_listener_internal_unittest.cc
    mpd_notify_muxer_listener_unittest.cc
    multi_codec_muxer_listener_unittest.cc
    muxer_listener_test_helper.cc
    vod_media_info_dump_muxer_listener_unittest.cc
)
target_link_libraries(media_event_unittest
    file
    file_test_util
    mpd_mocks
    mpd_media_info_proto
    gmock
    gtest
    gtest_main
    media_event
    mock_muxer_listener
)
add_gtest(media_event_unittest)
