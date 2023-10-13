# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_proto_library(widevine_protos STATIC
    widevine_common_encryption.proto
    widevine_pssh_data.proto)

add_library(media_base STATIC
    aes_cryptor.cc
    aes_decryptor.cc
    aes_encryptor.cc
    aes_pattern_cryptor.cc
    audio_stream_info.cc
    audio_timestamp_helper.cc
    bit_reader.cc
    bit_writer.cc
    buffer_reader.cc
    buffer_writer.cc
    byte_queue.cc
    cc_stream_filter.cc
    common_pssh_generator.cc
    container_names.cc
    decrypt_config.cc
    decryptor_source.cc
    http_key_fetcher.cc
    id3_tag.cc
    key_fetcher.cc
    key_source.cc
    language_utils.cc
    media_handler.cc
    media_sample.cc
    muxer.cc
    muxer_options.cc
    muxer_util.cc
    offset_byte_queue.cc
    playready_key_source.cc
    playready_pssh_generator.cc
    protection_system_specific_info.cc
    proto_json_util.cc
    pssh_generator.cc
    pssh_generator_util.cc
    raw_key_source.cc
    request_signer.cc
    rsa_key.cc
    stream_info.cc
    text_muxer.cc
    text_sample.cc
    text_stream_info.cc
    text_track_config.cc
    video_stream_info.cc
    video_util.cc
    widevine_key_source.cc
    widevine_pssh_generator.cc)

target_link_libraries(media_base
    absl::base
    absl::flags
    absl::log
    absl::str_format
    absl::strings
    file
    hex_parser
    mbedtls
    mpd_media_info_proto
    utils_clock
    status
    widevine_protos
    LibXml2)

add_library(media_handler_test_base STATIC
    media_handler_test_base.cc)
target_link_libraries(media_handler_test_base
    absl::log
    absl::strings
    gmock)

add_executable(media_base_unittest
    aes_cryptor_unittest.cc
    aes_pattern_cryptor_unittest.cc
    audio_timestamp_helper_unittest.cc
    bit_reader_unittest.cc
    bit_writer_unittest.cc
    buffer_writer_unittest.cc
    container_names_unittest.cc
    decryptor_source_unittest.cc
    http_key_fetcher_unittest.cc
    id3_tag_unittest.cc
    muxer_util_unittest.cc
    offset_byte_queue_unittest.cc
    producer_consumer_queue_unittest.cc
    protection_system_specific_info_unittest.cc
    pssh_generator_unittest.cc
    raw_key_source_unittest.cc
    rsa_key_unittest.cc
    test/rsa_test_data.cc
    video_util_unittest.cc
    widevine_key_source_unittest.cc)
target_link_libraries(media_base_unittest
    file
    file_test_util
    media_base
    gmock
    gtest
    gtest_main
    test_data_util
    test_web_server)
add_gtest(media_base_unittest)
