# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(media_origin STATIC
    origin_handler.cc)

target_link_libraries(media_origin
    absl::log
    status)
