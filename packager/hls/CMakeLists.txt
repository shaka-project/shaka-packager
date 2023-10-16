
# Copyright 2016 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd


add_library(hls_builder
  base/hls_notifier.h
  base/master_playlist.cc
  base/master_playlist.h
  base/media_playlist.cc
  base/media_playlist.h
  base/simple_hls_notifier.cc
  base/simple_hls_notifier.h
  base/tag.cc
  base/tag.h
  )

target_link_libraries(hls_builder
  absl::flags
  absl::log
  absl::strings
  absl::str_format
  file
  manifest_base
  media_base
  mpd_media_info_proto
  widevine_protos
  )

add_executable(hls_unittest
  base/master_playlist_unittest.cc
  base/media_playlist_unittest.cc
  base/mock_media_playlist.cc
  base/mock_media_playlist.h
  base/simple_hls_notifier_unittest.cc
  )

target_link_libraries(hls_unittest
  absl::flags
  file
  file_test_util
  gmock
  gtest
  gtest_main
  hls_builder
  test_data_util)

add_test(NAME hls_unittest COMMAND hls_unittest)
