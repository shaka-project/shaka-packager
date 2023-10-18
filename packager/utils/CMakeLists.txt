# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(utils_clock STATIC
  clock.h
  clock.cc)

add_library(utils_test_clock STATIC
  test_clock.cc
  test_clock.h)

target_link_libraries(utils_test_clock utils_clock absl::strings)

add_library(hex_parser STATIC
    hex_parser.cc)

target_link_libraries(hex_parser
    absl::strings)

add_library(hex_bytes_flags STATIC
  absl_flag_hexbytes.cc
  absl_flag_hexbytes.h)

target_link_libraries(hex_bytes_flags
  hex_parser
  absl::strings
  absl::flags)

add_library(string_utils STATIC
  string_trim_split.cc
)
target_link_libraries(string_utils
  absl::strings
)
