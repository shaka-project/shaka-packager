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

# Make sure protoc links against the same MSVC runtime as internal libs.
set(protobuf_MSVC_STATIC_RUNTIME OFF)

# Disable internal debugging features.
add_definitions(-DNDEBUG)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
