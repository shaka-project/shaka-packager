# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

execute_process(
    COMMAND python3 generate_license_notice.py "${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE STATUS)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "Failed to generate Packager license notice")
endif()

add_library(license_notice STATIC
    "${CMAKE_CURRENT_BINARY_DIR}/license_notice.cc")

# Anyone who depends on this library will need this include directory.
target_include_directories(license_notice PUBLIC "${CMAKE_BINARY_DIR}")

add_subdirectory(pssh)
