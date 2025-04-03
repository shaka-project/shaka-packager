# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(pluto_emsg STATIC
    emsg_ad_beacon.cc
    hasher.cc
)

target_include_directories(pluto_emsg PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(pluto_emsg 
    absl::base
    absl::log
    mbedtls)
