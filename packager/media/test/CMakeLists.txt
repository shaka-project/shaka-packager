# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(test_data_util STATIC
    test_data_util.cc)
target_link_libraries(test_data_util
    glog)
add_library(test_web_server STATIC
    test_web_server.cc)
target_link_libraries(test_web_server
    absl::str_format
    absl::strings
    mongoose
    nlohmann_json)
