# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(media_crypto STATIC
        aes_encryptor_factory.cc
        encryption_handler.cc
        sample_aes_ec3_cryptor.cc
        subsample_generator.cc)
target_link_libraries(media_crypto
        absl::base
        absl::log
        media_base
        media_codecs)

add_executable(media_crypto_unittest
        encryption_handler_unittest.cc
        sample_aes_ec3_cryptor_unittest.cc
        subsample_generator_unittest.cc)
target_link_libraries(media_crypto_unittest
        media_base
        media_codecs
        media_crypto
        media_handler_test_base
        status
        gmock
        gtest
        gtest_main)
add_gtest(media_crypto_unittest)
