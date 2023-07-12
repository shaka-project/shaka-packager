add_library(mpd_base_xml STATIC
  xml_node.cc
  )

target_link_libraries(mpd_base_xml
  gflags
  absl::base
  absl::flags
  absl::str_format
  absl::strings
  LibXml2
  )

add_executable(mpd_base_xml_unittest
  xml_node_unittest.cc)

target_link_libraries(mpd_base_xml_unittest
  file
  file_test_util
  mpd_base_xml
  gmock
  gtest
  gtest_main
  test_data_util)
add_test(NAME mpd_base_xml_unittest COMMAND mpd_base_xml_unittest)