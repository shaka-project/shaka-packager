# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_subdirectory(base)

add_library(manifest_base STATIC
  base/bandwidth_estimator.cc
  base/bandwidth_estimator.h
  )

target_link_libraries(manifest_base
  absl::log
  )

add_library(mpd_builder STATIC
  base/adaptation_set.cc
  base/adaptation_set.h
  base/content_protection_element.cc
  base/content_protection_element.h
  base/mpd_builder.cc
  base/mpd_builder.h
  base/mpd_notifier_util.cc
  base/mpd_notifier_util.h
  base/mpd_notifier.h
  base/mpd_options.h
  base/mpd_utils.cc
  base/mpd_utils.h
  base/period.cc
  base/period.h
  base/representation.cc
  base/representation.h
  base/segment_info.h
  base/simple_mpd_notifier.cc
  base/simple_mpd_notifier.h
  base/xml/scoped_xml_ptr.h
  base/xml/xml_node.cc
  base/xml/xml_node.h
  )

target_link_libraries(mpd_builder
  absl::flags
  absl::log
  absl::strings
  absl::str_format
  file
  LibXml2
  manifest_base
  media_base
  mpd_media_info_proto
  utils_clock
)


add_library(mpd_mocks STATIC
  base/mock_mpd_builder.cc
  base/mock_mpd_builder.h
  base/mock_mpd_notifier.cc
  base/mock_mpd_notifier.h
  )

target_link_libraries(mpd_mocks gmock absl::synchronization LibXml2 mpd_media_info_proto)

add_executable(mpd_unittest
  base/adaptation_set_unittest.cc
  base/bandwidth_estimator_unittest.cc
  base/mpd_builder_unittest.cc
  base/mpd_utils_unittest.cc
  base/period_unittest.cc
  base/representation_unittest.cc
  base/simple_mpd_notifier_unittest.cc
  base/xml/xml_node_unittest.cc
  test/mpd_builder_test_helper.cc
  test/mpd_builder_test_helper.h
  test/xml_compare.cc
  test/xml_compare.h
  util/mpd_writer_unittest.cc
  )

target_compile_definitions(mpd_unittest
  PRIVATE
  # We used to build off of __FILE__, but that is not always an absolute
  # path, depending on the version of CMake.  This is consistent.
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/data"
  TEST_SCHEMA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/schema")

target_link_libraries(mpd_unittest
  file
  file_test_util
  test_data_util
  absl::flags
  mpd_builder
  mpd_mocks
  mpd_util
  gmock
  gtest
  utils_test_clock
  gtest_main)

add_test(NAME mpd_unittest COMMAND mpd_unittest)

add_library(mpd_util STATIC
  util/mpd_writer.cc
  util/mpd_writer.h)

target_link_libraries(mpd_util
  file
  absl::flags
  mpd_builder
  mpd_mocks
  )

