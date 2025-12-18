# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

execute_process(
    COMMAND "${Python3_EXECUTABLE}" generate_version_string.py
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE STATUS
    OUTPUT_VARIABLE PACKAGER_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ENCODING UTF8)
if(NOT STATUS EQUAL 0)
  message(FATAL_ERROR "Failed to generate Packager version")
endif()

add_library(version STATIC version.cc)
target_compile_definitions(version PRIVATE
    PACKAGER_VERSION="${PACKAGER_VERSION}")
target_link_libraries(version
    absl::synchronization)
