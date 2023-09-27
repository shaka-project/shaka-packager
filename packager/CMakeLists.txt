# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Packager CMake build file.

# Build static libs by default, or shared if LIBPACKAGER_SHARED is defined.
if(LIBPACKAGER_SHARED)
  add_definitions(-DSHARED_LIBRARY_BUILD)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Global C++ flags.
if(MSVC)
  # Warning level 4 and all warnings as errors.
  add_compile_options(/W4 /WX)
  # Silence a warning from an absl header about alignment in boolean flags.
  add_compile_options(/wd4324)
  # Silence a warning about STL types in exported classes.
  add_compile_options(/wd4251)
  # Silence a warning about constant conditional expressions.
  add_compile_options(/wd4127)
  # We use long-jumps in subtitle_composer.cc due to API of libpng
  add_compile_options(/wd4611)
  # need /bigobj for box definitions
  add_compile_options(/bigobj)

  # Packager's macro for Windows-specific code.
  add_definitions(-DOS_WIN)

  # Suppress Microsoft's min() and max() macros, which will conflict with
  # things like std::numeric_limits::max() and std::min().
  add_definitions(-DNOMINMAX)
  # Define this so that we can use fopen() without warnings.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  # Don't automatically include winsock.h in windows.h.  This is needed for us
  # to use winsock2.h, which contains definitions that conflict with the
  # ancient winsock 1.1 interface in winsock.h.
  add_definitions(-DWIN32_LEAN_AND_MEAN)
else()
  # Lots of warnings and all warnings as errors.
  # Note that we can't use -Wpedantic due to absl's int128 headers.
  add_compile_options(-Wall -Wextra -Werror)
  # Several warning suppression flags are required on one compiler version and
  # not understood by another.  Do not treat these as errors.
  add_compile_options(-Wno-unknown-warning-option)
endif()

# Global include paths.
# Project root, to reference internal headers as packager/foo/bar/...
include_directories(..)
# Public include folder, to reference public headers as packager/foo.h
include_directories(../include)

# Include our module for gtest-based testing.
include("gtest.cmake")

# Include our module for building protos.
include("protobuf.cmake")

# Subdirectories with their own CMakeLists.txt, all of whose targets are built.
add_subdirectory(file)
add_subdirectory(kv_pairs)
add_subdirectory(media)
add_subdirectory(hls)
add_subdirectory(mpd)
add_subdirectory(status)
add_subdirectory(third_party)
add_subdirectory(tools)
add_subdirectory(utils)
add_subdirectory(version)

set(libpackager_sources
  app/job_manager.cc
  app/job_manager.h
  app/muxer_factory.cc
  app/muxer_factory.h
  app/packager_util.cc
  app/packager_util.h
  app/single_thread_job_manager.cc
  app/single_thread_job_manager.h
  packager.cc
  ../include/packager/packager.h
  )

if(LIBPACKAGER_SHARED)
  add_library(libpackager SHARED ${libpackager_sources})
  target_compile_definitions(libpackager PUBLIC SHAKA_IMPLEMENTATION)
else()
  add_library(libpackager STATIC ${libpackager_sources})
endif()

set(LIBPACKAGER_HEADER_FILES
  file/file.h
  macros.h
  packager.h
  status/status.h
)

target_link_libraries(libpackager
  file
  hls_builder
  media_chunking
  media_codecs
  media_crypto
  demuxer
  media_event
  dvb
  mp2t
  mp4
  packed_audio
  ttml
  formats_webm
  wvm
  media_replicator
  media_trick_play
  mpd_builder
  mbedtls
  string_utils
  version
)
set_target_properties(libpackager PROPERTIES PUBLIC_HEADER "${LIBPACKAGER_HEADER_FILES}")
if(NOT MSVC)
  set_property(TARGET libpackager PROPERTY OUTPUT_NAME packager)
endif()

add_executable(packager
  app/ad_cue_generator_flags.cc
  app/ad_cue_generator_flags.h
  app/crypto_flags.cc
  app/crypto_flags.h
  app/hls_flags.cc
  app/hls_flags.h
  app/manifest_flags.cc
  app/manifest_flags.h
  app/mpd_flags.cc
  app/mpd_flags.h
  app/muxer_flags.cc
  app/muxer_flags.h
  app/packager_main.cc
  app/playready_key_encryption_flags.cc
  app/playready_key_encryption_flags.h
  app/raw_key_encryption_flags.cc
  app/raw_key_encryption_flags.h
  app/protection_system_flags.cc
  app/protection_system_flags.h
  app/retired_flags.cc
  app/retired_flags.h
  app/stream_descriptor.cc
  app/stream_descriptor.h
  app/validate_flag.cc
  app/validate_flag.h
  app/vlog_flags.cc
  app/vlog_flags.h
  app/widevine_encryption_flags.cc
  app/widevine_encryption_flags.h
)
target_link_libraries(packager
  absl::flags
  absl::flags_parse
  absl::log
  absl::log_flags
  absl::strings
  hex_bytes_flags
  libpackager
  license_notice
  string_utils
)

add_executable(mpd_generator
  app/mpd_generator.cc
  app/mpd_generator_flags.h
  app/vlog_flags.cc
  app/vlog_flags.h
)
target_link_libraries(mpd_generator
  absl::flags
  absl::flags_parse
  absl::log
  absl::log_flags
  absl::strings
  license_notice
  mpd_builder
  mpd_util
  )

add_executable(packager_test
  packager_test.cc
  )
target_link_libraries(packager_test
  libpackager
  gmock
  gtest
  gtest_main)

configure_file(packager.pc.in packager.pc @ONLY)


install(TARGETS mpd_generator packager libpackager
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES tools/pssh/pssh-box.py DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_BINARY_DIR}/packager/packager.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
