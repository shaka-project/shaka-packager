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

# Disable this error/warning, which occurs in
# src/google/protobuf/stubs/stringprintf.cc:
add_compile_options(-Wno-sign-compare)
# Disable this error/warning, which occurs in
# src/google/protobuf/compiler/cpp/parse_function_generator.cc:
add_compile_options(-Wno-missing-field-initializers)
# There are several interfaces with ununused parameters.
add_compile_options(-Wno-unused-parameter)
# There are also redundant move calls.
add_compile_options(-Wno-redundant-move)
# Disable internal debugging features, which end up triggering further compiler
# errors.
add_definitions(-DNDEBUG)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
