# Copyright 2020 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd


# Copyright 2016 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd


add_library(dvb STATIC
  dvb_image.cc
  dvb_image.h
  dvb_sub_parser.cc
  dvb_sub_parser.h
  subtitle_composer.cc
  subtitle_composer.h
  )
target_link_libraries(dvb
  absl::flags
  absl::log
  absl::strings
  absl::str_format
  file
  manifest_base
  media_base
  mpd_media_info_proto
  png_static
  widevine_protos
  )

add_executable(dvb_unittest
  dvb_image_unittest.cc
  dvb_sub_parser_unittest.cc
  subtitle_composer_unittest.cc
  )

target_link_libraries(dvb_unittest
  file
  file_test_util
  test_data_util
  absl::flags
  media_event
  dvb
  gmock
  gtest
  gtest_main)

add_test(NAME dvb_unittest COMMAND dvb_unittest)
