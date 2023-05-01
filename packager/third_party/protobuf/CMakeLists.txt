# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host protobuf configuration.

# Turn these features off.
set(protobuf_INSTALL OFF)
set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_CONFORMANCE OFF)
set(protobuf_BUILD_EXAMPLES OFF)
set(protobuf_BUILD_LIBPROTOC OFF)
set(protobuf_BUILD_SHARED_LIBS OFF)
set(protobuf_WITH_ZLIB OFF)

# Turn these features on.
set(protobuf_BUILD_PROTOC_BINARIES ON)
set(protobuf_DISABLE_RTTI ON)

# The latest version of protobuf requires a path to ABSL.
set(ABSL_ROOT_DIR get_filename_component(ABSOLUTE_PATH ../abseil-cpp/source ABSOLUTE))

# Disable these errors/warnings:
if(MSVC)
  add_compile_options(
    # src/google/protobuf/arena_align.h
    /wd4146  # sign comparison
    # src/google/protobuf/generated_message_tctable_lite.cc
    /wd4141  # multiple inline keywords
  )
else()
  add_compile_options(
    # src/google/protobuf/util/message_differencer.cc
    -Wno-type-limits
    # src/google/protobuf/stubs/stringprintf.cc
    -Wno-sign-compare
    # src/google/protobuf/compiler/cpp/parse_function_generator.cc
    -Wno-missing-field-initializers
    # src/google/protobuf/message_lite.cc
    -Wno-stringop-overflow
    # src/google/protobuf/stubs/int128.cc
    -Wno-shorten-64-to-32
    # src/google/protobuf/generated_message_tctable_lite.cc
    -Wno-unused-function

    # There are several interfaces with ununused parameters.
    -Wno-unused-parameter
    # There are also redundant move calls.
    -Wno-redundant-move
  )
endif()

# Disable internal debugging features, which end up triggering further compiler
# errors.
add_definitions(-DNDEBUG)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)